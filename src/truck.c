/*** truffle.c -- tool to apply roll-over directives
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of truffle.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "truffle.h"
#include "wheap.h"
#include "nifty.h"
#include "coru.h"
#include "trod.h"
#include "mmy.h"
#include "truf-dfp754.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:1572)
#endif /* __INTEL_COMPILER */

struct truf_ctx_s {
	truf_wheap_t q;

	unsigned int edgp:1U;
	unsigned int relp:1U;
	unsigned int absp:1U;
	unsigned int ocop:1U;
};


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static size_t
xstrlcpy(char *restrict dst, const char *src, size_t dsz)
{
	size_t ssz = strlen(src);
	if (ssz > dsz) {
		ssz = dsz - 1U;
	}
	memcpy(dst, src, ssz);
	dst[ssz] = '\0';
	return ssz;
}


/* last price stacks */
static struct lstk_s {
	truf_trod_t d;
	truf_price_t last;
} lstk[64U];
static size_t imin;
static size_t imax;

static size_t
lstk_find(truf_mmy_t contract)
{
	for (size_t i = imin; i < imax; i++) {
		if (truf_mmy_eq_p(lstk[i].d.sym, contract)) {
			return i;
		}
	}
	return imax;
}

static void
lstk_kick(truf_trod_t directive)
{
	size_t i;

	if ((i = lstk_find(directive.sym)) >= imax) {
		return;
	}
	/* otherwise kick the i-th slot */
	lstk[i].d = truf_nul_trod();
	lstk[i].last = 0.df;
	if (i == imin) {
		/* up imin */
		for (size_t j = ++imin; j < imax; imin++, j++) {
			if (lstk[j].d.sym != 0U) {
				break;
			}
		}
	}
	if (i == imax - 1U) {
		/* down imax */
		for (size_t j = imax; --j >= imin; imax--) {
			if (lstk[j].d.sym != 0U) {
				break;
			}
		}
	}
	/* condense imin/imax, only if imin or imax reach into
	 * the other half of the lstack */
	if (imin >= countof(lstk) / 2U || imin && imax >= countof(lstk) / 2U) {
		memmove(lstk + 0U, lstk + imin, (imax - imin) * sizeof(*lstk));
		imax -= imin;
		imin = 0;
	}
	return;
}

static void
lstk_join(truf_trod_t directive)
{
	size_t i;

	if ((i = lstk_find(directive.sym)) < imax) {
		/* already in there, just fuck off */
		return;
	}
	/* otherwise add */
	lstk[imax].d = directive;
	lstk[imax].last = nand32(NULL);
	imax++;
	return;
}

static bool
relevantp(size_t i)
{
	return i < imax;
}

static __attribute__((unused)) void
lstk_prnt(void)
{
	char buf[256U];
	const char *const ep = buf + sizeof(buf);

	for (size_t i = imin; i < imax; i++) {
		char *bp = buf;
		truf_trod_wr(bp, ep - bp, lstk[i].d);
		puts(buf);
	}
	return;
}


/* trod directives cache */
static truf_trod_t *trods;
static size_t ntrods;
static size_t trodi;

static int
truf_add_trod(struct truf_ctx_s ctx[static 1U], echs_instant_t t, truf_trod_t d)
{
	uintptr_t qmsg;

	/* resize check */
	if (trodi >= ntrods) {
		size_t nu = ntrods + 64U;
		trods = realloc(trods, nu * sizeof(*trods));
		ntrods = nu;
	}

	/* `clone' D */
	qmsg = (uintptr_t)trodi;
	trods[trodi++] = d;
	/* insert to heap */
	truf_wheap_add_deferred(ctx->q, t, qmsg);
	return 0;
}

static void
truf_free_trods(void)
{
	if (trods != NULL) {
		free(trods);
	}
	trods = NULL;
	ntrods = 0U;
	trodi = 0U;
	return;
}


declcoru(co_echs_rdr, {
		FILE *f;
	}, {});

static const struct co_rdr_res_s {
	echs_instant_t t;
	const char *ln;
	size_t lz;
} *defcoru(co_echs_rdr, c, UNUSED(arg))
{
/* coroutine for the reader of the tseries */
	char *line = NULL;
	size_t llen = 0UL;
	ssize_t nrd;
	/* we'll yield a rdr_res */
	struct co_rdr_res_s res;

	while ((nrd = getline(&line, &llen, c->f)) > 0) {
		char *p;

		if (*line == '#') {
			continue;
		} else if (echs_instant_0_p(res.t = dt_strp(line, &p))) {
			continue;
		} else if (*p != '\t') {
			;
		} else {
			/* fast forward a bit */
			p++;
		}
		/* pack the result structure */
		res.ln = p;
		res.lz = nrd - (p - line);
		yield(res);
	}

	free(line);
	line = NULL;
	llen = 0U;
	return 0;
}

declcoru(co_echs_pop, {
		truf_wheap_t q;
	}, {});

static const struct co_pop_res_s {
	echs_instant_t t;
	truf_trod_t edge;
} *defcoru(co_echs_pop, c, UNUSED(arg))
{
/* coroutine for the wheap popper */
	/* we'll yield a pop_res */
	struct co_pop_res_s res;

	while (!echs_instant_0_p(res.t = truf_wheap_top_rank(c->q))) {
		/* assume it's a truf_trod_t */
		uintptr_t tmp = truf_wheap_pop(c->q);
		res.edge = trods[tmp];
		yield(res);
		if (!truf_mmy_p(res.edge.sym) && res.edge.sym) {
			free((char*)res.edge.sym);
		}
	}
	return 0;
}

declcoru(co_inst_rdr, {
		const char *const *dt;
		size_t ndt;
	}, {});

static const struct co_rdr_res_s*
defcoru(co_inst_rdr, c, UNUSED(arg))
{
/* coroutine for the reader of the tseries */
	const char *const *dt = c->dt;
	const char *const *const edt = c->dt + c->ndt;
	/* we'll yield a rdr_res */
	struct co_rdr_res_s res;

	for (; dt < edt; dt++) {
		char *on;
		res.t = dt_strp(*dt, &on);
		res.ln = *dt;
		res.lz = on - *dt;
		yield(res);
	}
	return 0;
}

declcoru(co_echs_out, {
		FILE *f;
		unsigned int relp:1U;
		unsigned int absp:1U;
		unsigned int ocop:1U;
	}, {
		echs_instant_t dt;
		uintptr_t sym;
		_Decimal32 val;
	});

static const void*
defcoru(co_echs_out, ia, arg)
{
	char buf[256U];
	const char *const ep = buf + sizeof(buf);

	while (arg != NULL) {
		char *bp = buf;
		echs_instant_t t = arg->dt;

		bp += dt_strf(bp, ep - bp, t);
		*bp++ = '\t';
		if (UNLIKELY(arg->sym == 0U)) {
			;
		} else if (!truf_mmy_p(arg->sym)) {
			bp += xstrlcpy(bp, (const void*)arg->sym, ep - bp);
		} else {
			truf_mmy_t sym;

			if (ia->ocop) {
				sym = truf_mmy_oco(arg->sym, t.y);
			} else if (ia->absp) {
				sym = truf_mmy_abs(arg->sym, t.y);
			} else if (ia->relp) {
				sym = truf_mmy_rel(arg->sym, t.y);
			} else {
				sym = arg->sym;
			}
			bp += truf_mmy_wr(bp, ep - bp, sym);
		}
		*bp++ = '\t';
		bp += d32tostr(bp, ep - bp, arg->val);
		*bp++ = '\n';
		*bp = '\0';
		fputs(buf, ia->f);

		arg = yield_ptr(NULL);
	}
	return 0;
}

declcoru(co_tser_edg, {
		truf_wheap_t q;
		FILE *tser;
	}, {});

static const struct co_edg_res_s {
	echs_instant_t t;
	truf_mmy_t c;
	truf_price_t last;
	_Decimal32 exp_ol;
	_Decimal32 exp_nu;
} *defcoru(co_tser_edg, ia, UNUSED(arg))
{
/* yields a co_edg_res when exposure changes */
	coru_t rdr;
	coru_t pop;
	struct co_edg_res_s res;

	init_coru();
	rdr = make_coru(co_echs_rdr, ia->tser);
	pop = make_coru(co_echs_pop, ia->q);

	const struct co_pop_res_s *ev;
	const struct co_rdr_res_s *ln;
	for (ln = next(rdr), ev = next(pop); ln != NULL;) {
		/* sum up caevs in between price lines */
		for (;
		     LIKELY(ev != NULL) &&
			     UNLIKELY(!echs_instant_lt_p(ln->t, ev->t));
		     ev = next(pop)) {
			truf_mmy_t c = ev->edge.sym;
			size_t i;

			if (!truf_mmy_abs_p(c)) {
				c = truf_mmy_abs(c, ev->t.y);
			}
			if (ev->edge.exp == 0.df) {
				if (relevantp(i = lstk_find(c))) {
					/* yield edge and exposure */
					res.t = ev->t;
					res.c = c;
					res.exp_ol = lstk[i].d.exp;
					res.exp_nu = 0.df;
					res.last = lstk[i].last;
					yield(res);
				}
				lstk_kick((truf_trod_t){c, ev->edge.exp});
			} else {
				lstk_join((truf_trod_t){c, ev->edge.exp});
				if (relevantp(i = lstk_find(c))) {
					/* yield edge and exposure */
					res.t = ev->t;
					res.c = c;
					res.exp_ol = lstk[i].d.exp;
					res.exp_nu = ev->edge.exp;
					res.last = lstk[i].last;
					yield(res);
				}
			}
		}

		/* apply roll-over directives to price lines */
		do {
			char *on;
			truf_mmy_t c = truf_mmy_rd(ln->ln, &on);
			size_t i;

			if (!truf_mmy_abs_p(c)) {
				c = truf_mmy_abs(c, ln->t.y);
			}
			if (relevantp(i = lstk_find(c))) {
				/* keep track of last price */
				truf_price_t p = strtod32(on, &on);
				bool prntp = isnand32(lstk[i].last);

				/* keep track of last price */
				lstk[i].last = p;
				/* yield edge and exposure */
				if (UNLIKELY(prntp)) {
					res.t = ln->t;
					res.c = c;
					res.exp_ol = lstk[i].d.exp;
					res.exp_nu = lstk[i].d.exp;
					res.last = p;
					yield(res);
				}
			}
		} while (LIKELY((ln = next(rdr)) != NULL) &&
			 (UNLIKELY(ev == NULL) ||
			  LIKELY(echs_instant_lt_p(ln->t, ev->t))));
	}

	free_coru(rdr);
	free_coru(pop);
	fini_coru();
	return 0;
}

declcoru(co_tser_lev, {
		truf_wheap_t q;
		FILE *tser;
	}, {});

static const struct co_edg_res_s*
defcoru(co_tser_lev, ia, UNUSED(arg))
{
/* yields a co_edg_res whenever a tick in the time series occurs */
	coru_t rdr;
	coru_t pop;
	struct co_edg_res_s res;

	init_coru();
	rdr = make_coru(co_echs_rdr, ia->tser);
	pop = make_coru(co_echs_pop, ia->q);

	const struct co_pop_res_s *ev;
	const struct co_rdr_res_s *ln;
	for (ln = next(rdr), ev = next(pop); ln != NULL;) {
		/* sum up caevs in between price lines */
		for (;
		     LIKELY(ev != NULL) &&
			     UNLIKELY(!echs_instant_lt_p(ln->t, ev->t));
		     ev = next(pop)) {
			truf_mmy_t c = ev->edge.sym;

			if (!truf_mmy_abs_p(c)) {
				c = truf_mmy_abs(c, ev->t.y);
			}
			if (ev->edge.exp == 0.df) {
				lstk_kick((truf_trod_t){c, ev->edge.exp});
			} else {
				lstk_join((truf_trod_t){c, ev->edge.exp});
			}
		}

		/* apply roll-over directives to price lines */
		do {
			char *on;
			truf_mmy_t c = truf_mmy_rd(ln->ln, &on);
			size_t i;

			if (!truf_mmy_abs_p(c)) {
				c = truf_mmy_abs(c, ln->t.y);
			}
			if (relevantp(i = lstk_find(c))) {
				/* keep track of last price */
				truf_price_t p = strtod32(on, &on);

				/* yield edge and exposure */
				res.t = ln->t;
				res.c = c;
				res.exp_ol = lstk[i].d.exp;
				res.exp_nu = lstk[i].d.exp;
				res.last = p;
				yield(res);
			}
		} while (LIKELY((ln = next(rdr)) != NULL) &&
			 (UNLIKELY(ev == NULL) ||
			  LIKELY(echs_instant_lt_p(ln->t, ev->t))));
	}

	free_coru(rdr);
	free_coru(pop);
	fini_coru();
	return 0;
}

declcoru(co_echs_pos, {
		truf_wheap_t q;
		const char *const *dt;
		size_t ndt;
	}, {});

static coru_argp(co_echs_out)
defcoru(co_echs_pos, ia, UNUSED(arg))
{
/* yields something that co_echs_out can use directly */
	coru_t rdr;
	coru_t pop;
	coru_args(co_echs_out) res;

	init_coru();
	if (ia->ndt == 0U) {
		/* read date/times from stdin */
		rdr = make_coru(co_echs_rdr, stdin);
	} else {
		rdr = make_coru(co_inst_rdr, ia->dt, ia->ndt);
	}
	pop = make_coru(co_echs_pop, ia->q);

	const struct co_pop_res_s *ev;
	const struct co_rdr_res_s *ln;
	for (ln = next(rdr), ev = next(pop); ln != NULL;) {
		/* in between date/times from the RDR find trods */
		for (;
		     LIKELY(ev != NULL) &&
			     UNLIKELY(!echs_instant_lt_p(ln->t, ev->t));
		     ev = next(pop)) {
			truf_mmy_t c = ev->edge.sym;

			if (ev->edge.exp == 0.df) {
				lstk_kick((truf_trod_t){c, ev->edge.exp});
			} else {
				lstk_join((truf_trod_t){c, ev->edge.exp});
			}
		}

		do {
			char buf[1024U];
			char *bp = buf;
			const char *const ep = buf + sizeof(buf);

			bp += dt_strf(bp, ep - bp, ln->t);
			*bp++ = '\t';
			for (size_t i = imin; i < imax; i++) {
				if (lstk[i].d.sym == 0U) {
					continue;
				}
				/* otherwise prep the yield */
				res.dt = ln->t;
				res.sym = lstk[i].d.sym;
				res.val = lstk[i].d.exp;
				yield(res);
			}
		} while (LIKELY((ln = next(rdr)) != NULL) &&
			 (UNLIKELY(ev == NULL) ||
			  LIKELY(echs_instant_lt_p(ln->t, ev->t))));
	}

	free_coru(rdr);
	free_coru(pop);
	fini_coru();
	return 0;
}


/* public api, might go to libtruffle one day */
static int
truf_read_trod_file(struct truf_ctx_s ctx[static 1U], const char *fn)
{
/* wants a const char *fn */
	coru_t rdr;
	FILE *f;

	if (fn == NULL) {
		f = stdin;
	} else if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
		return -1;
	}

	init_coru();
	rdr = make_coru(co_echs_rdr, f);

	for (const struct co_rdr_res_s *ln; (ln = next(rdr)) != NULL;) {
		/* try to read the whole shebang */
		truf_trod_t c = truf_trod_rd(ln->ln, NULL);
		/* ... and add it */
		truf_add_trod(ctx, ln->t, c);
	}
	/* now sort the guy */
	truf_wheap_fix_deferred(ctx->q);

	free_coru(rdr);
	fini_coru();
	fclose(f);
	return 0;
}

static void
truf_prnt_trod_file(struct truf_ctx_s ctx[static 1U], FILE *f)
{
	for (echs_instant_t t;
	     !echs_instant_0_p(t = truf_wheap_top_rank(ctx->q));) {
		uintptr_t tmp = truf_wheap_pop(ctx->q);
		truf_trod_t this = trods[tmp];
		char buf[256U];
		char *bp = buf;
		const char *const ep = buf + sizeof(buf);

		bp += dt_strf(bp, ep - bp, t);
		*bp++ = '\t';
		if (ctx->ocop) {
			this.sym = truf_mmy_oco(this.sym, t.y);
		} else if (ctx->absp) {
			this.sym = truf_mmy_abs(this.sym, t.y);
		} else if (ctx->relp) {
			this.sym = truf_mmy_rel(this.sym, t.y);
		}
		bp += truf_trod_wr(bp, ep - bp, this);
		*bp++ = '\n';
		*bp = '\0';
		fputs(buf, f);
		if (!truf_mmy_p(this.sym) && this.sym) {
			free((char*)this.sym);
		}
	}
	return;
}


/* old schema wizardry */
struct cnode_s {
	idate_t x;
	daisy_t l;
	double y __attribute__((aligned(sizeof(double))));
};

struct cline_s {
	daisy_t valid_from;
	daisy_t valid_till;
	char month;
	int8_t year_off;
	size_t nn;
	struct cnode_s n[];
};

struct trsch_s {
	size_t np;
	struct cline_s *p[];
};

static inline unsigned int
m_to_i(char month)
{
	switch (month) {
	case 'f': case 'F':
		return 1U;
	case 'g': case 'G':
		return 2U;
	case 'h': case 'H':
		return 3U;
	case 'j': case 'J':
		return 4U;
	case 'k': case 'K':
		return 5U;
	case 'm': case 'M':
		return 6U;
	case 'n': case 'N':
		return 7U;
	case 'q': case 'Q':
		return 8U;
	case 'u': case 'U':
		return 9;
	case 'v': case 'V':
		return 10U;
	case 'x': case 'X':
		return 11U;
	case 'z': case 'Z':
		return 12U;
	default:
		break;
	}
	return 0U;
}

static truf_trod_t
make_trod_from_cline(const struct cline_s *p, daisy_t when)
{
	unsigned int y = daisy_to_year(when);
	truf_trod_t res;

	for (size_t j = 0; j < p->nn - 1; j++) {
		const struct cnode_s *n1 = p->n + j;
		const struct cnode_s *n2 = n1 + 1;
		daisy_t l1 = daisy_in_year(n1->l, y);
		daisy_t l2 = daisy_in_year(n2->l, y);

		if (when == l2) {
			/* something happened at l2 */
			if (n2->y == 0.0 && n1->y != 0.0) {
				res.exp = 0.df;
			} else if (n2->y != 0.0 && n1->y == 0.0) {
				res.exp = 1.df;
			} else {
				continue;
			}
		} else if (j == 0 && when == l1) {
			/* something happened at l1 */
			if (UNLIKELY(n1->y != 0.0)) {
				res.exp = 1.df;
			} else {
				continue;
			}
		} else {
			continue;
		}
		res.sym = make_truf_mmy(y + p->year_off, m_to_i(p->month), 0U);

		/* indicate success (as in clear for adding) */
		return res;
	}
	/* indicate failure (to add anything) */
	return truf_nul_trod();
}

static void
bang_schema(struct truf_ctx_s ctx[static 1], trsch_t sch, daisy_t when)
{
	echs_instant_t t = daisy_to_instant(when);

	for (size_t i = 0; i < sch->np; i++) {
		const struct cline_s *p = sch->p[i];
		truf_trod_t d;

		/* check year validity */
		if (when < p->valid_from || when > p->valid_till) {
			/* cline isn't applicable */
			;
		} else if (!((d = make_trod_from_cline(p, when)).sym)) {
			/* nothing added then */
			;
		} else {
			/* just add the guy */
			truf_add_trod(ctx, t, d);
		}
	}
	/* and sort the guy */
	truf_wheap_fix_deferred(ctx->q);
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "truck.xh"
#include "truck.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

static int
cmd_print(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "Usage: truffle print [TROD-FILE]...\n";
	static struct truf_ctx_s ctx[1];
	int res = 0;

	if (argi->inputs_num < 1U) {
		fputs(usg, stderr);
		res = 1;
		goto out;
	} else if (UNLIKELY((ctx->q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	if (argi->oco_given) {
		ctx->ocop = 1U;
	} else if (argi->abs_given) {
		ctx->absp = 1U;
	} else if (argi->rel_given) {
		ctx->relp = 1U;
	}

	for (unsigned int i = 1U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(ctx, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}
	/* and print him */
	truf_prnt_trod_file(ctx, stdout);

out:
	if (LIKELY(ctx->q != NULL)) {
		free_truf_wheap(ctx->q);
	}
	truf_free_trods();
	return res;
}

static int
cmd_migrate(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "Usage: truffle migrate [SCHEMA-FILE]...\n";
	static struct truf_ctx_s ctx[1];
	daisy_t from;
	daisy_t till;
	int res = 0;

	if (argi->inputs_num < 1U) {
		fputs(usg, stderr);
		res = 1;
		goto out;
	} else if (UNLIKELY((ctx->q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	if (argi->from_given) {
		echs_instant_t i = dt_strp(argi->from_arg, NULL);
		from = instant_to_daisy(i);
	} else {
		echs_instant_t i = {2000U, 1U, 1U, ECHS_ALL_DAY};
		from = instant_to_daisy(i);
	}
	if (argi->till_given) {
		echs_instant_t i = dt_strp(argi->till_arg, NULL);
		till = instant_to_daisy(i);
	} else {
		echs_instant_t i = {2037U, 12U, 31U, ECHS_ALL_DAY};
		till = instant_to_daisy(i);
	}

	for (size_t i = 1U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];
		trsch_t sch;

		if ((sch = read_schema(fn)) == NULL) {
			/* try the normal trod reader */
			(void)truf_read_trod_file(ctx, fn);
			continue;
		}
		/* bang into wheap */
		for (daisy_t now = from; now <= till; now++) {
			bang_schema(ctx, sch, now);
		}
		/* and out again */
		free_schema(sch);
	}

	/* and print the whole wheap now */
	truf_prnt_trod_file(ctx, stdout);

out:
	if (LIKELY(ctx->q != NULL)) {
		free_truf_wheap(ctx->q);
	}
	truf_free_trods();
	return res;
}

static int
cmd_filter(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "\
Usage: truffle filter TSER-FILE [TROD-FILE]...\n";
	static struct truf_ctx_s ctx[1];
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usg, stderr);
		res = 1;
		goto out;
	} else if (UNLIKELY((ctx->q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	for (unsigned int i = 2U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(ctx, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}

	with (const char *fn = argi->inputs[1U]) {
		coru_t edg;
		coru_t out;
		FILE *f;

		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			res = 1;
			goto out;
		}

		init_coru();
		if (argi->edge_given) {
			edg = make_coru(co_tser_edg, ctx->q, f);
		} else {
			edg = make_coru(co_tser_lev, ctx->q, f);
		}
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_given, argi->abs_given, argi->oco_given);

		for (const struct co_edg_res_s *e; (e = next(edg)) != NULL;) {
			if (UNLIKELY(isnand32(e->last))) {
				continue;
			}
			____next(out, e);
		}

		free_coru(edg);
		free_coru(out);
		fini_coru();
	}
out:
	if (LIKELY(ctx->q != NULL)) {
		free_truf_wheap(ctx->q);
	}
	truf_free_trods();
	return res;
}

static int
cmd_position(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "\
Usage: truffle position TROD-FILE [DATE/TIME]...\n";
	truf_wheap_t q;
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usg, stderr);
		res = 1;
		goto out;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	with (const char *fn = argi->inputs[1U]) {
		struct truf_ctx_s ctx = {q};
		if (UNLIKELY(truf_read_trod_file(&ctx, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}

	/* just print them now */
	{
		const char *const *dt = argi->inputs + 2U;
		const size_t ndt = argi->inputs_num - 2U;
		coru_t pos;
		coru_t out;

		init_coru();
		pos = make_coru(co_echs_pos, q, dt, ndt);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_given, argi->abs_given, argi->oco_given);

		for (const struct co_edg_out_s *e; (e = next(pos)) != NULL;) {
			____next(out, e);
		}

		free_coru(edg);
		free_coru(out);
		fini_coru();
	}

out:
	if (LIKELY(q != NULL)) {
		free_truf_wheap(q);
	}
	truf_free_trods();
	return res;
}

int
main(int argc, char *argv[])
{
	struct truf_args_info argi[1];
	int res = 0;

	if (truf_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		goto nocmd;
	}

	/* get the coroutines going */
	init_coru_core();

	/* check the command */
	with (const char *cmd = argi->inputs[0U]) {
		if (!strcmp(cmd, "print")) {
			res = cmd_print(argi);
		} else if (!strcmp(cmd, "migrate")) {
			res = cmd_migrate(argi);
		} else if (!strcmp(cmd, "filter")) {
			res = cmd_filter(argi);
		} else if (!strcmp(cmd, "position")) {
			res = cmd_position(argi);
		} else {
		nocmd:
			error("No valid command specified.\n\
See --help to obtain a list of available commands.");
			res = 1;
			goto out;
		}
	}

out:
	/* just to make sure */
	fflush(stdout);
	truf_parser_free(argi);
	return res;
}

/* truffle.c ends here */
