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
#include "step.h"
#include "rpaf.h"
#include "truf-dfp754.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:1572)
#endif /* __INTEL_COMPILER */

typedef const struct truf_step_s *truf_step_cell_t;


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


/* trod directives cache */
static truf_trod_t *trods;
static size_t ntrods;
static size_t trodi;

static int
truf_add_trod(truf_wheap_t q, echs_instant_t t, truf_trod_t d)
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
	truf_wheap_add_deferred(q, t, qmsg);
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

static truf_step_cell_t
defcoru(co_echs_pop, c, UNUSED(arg))
{
/* coroutine for the wheap popper */
	/* we'll yield a pop_res */
	struct truf_step_s res = {
		.old = nand32(NULL),
	};

	while (!echs_instant_0_p(res.t = truf_wheap_top_rank(c->q))) {
		/* assume it's a truf_trod_t */
		uintptr_t tmp = truf_wheap_pop(c->q);
		res.sym = trods[tmp].sym;
		res.new = trods[tmp].exp;
		yield(res);
		if (!truf_mmy_p(res.sym) && res.sym) {
			free((char*)res.sym);
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
		unsigned int prnt_prcp:1U;
		unsigned int prnt_expp:1U;
	}, {});

static const void*
_defcoru(co_echs_out, ia, truf_step_cell_t arg)
{
	char buf[256U];
	const char *const ep = buf + sizeof(buf);

	while (arg != NULL) {
		char *bp = buf;
		echs_instant_t t = arg->t;

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
		if (ia->prnt_prcp) {
			if (!isnand32(arg->bid)) {
				*bp++ = '\t';
				bp += d32tostr(bp, ep - bp, arg->bid);
			}
			if (!isnand32(arg->ask)) {
				*bp++ = '\t';
				bp += d32tostr(bp, ep - bp, arg->ask);
			}
		}
		if (ia->prnt_expp) {
			*bp++ = '\t';
			if (!isnand32(arg->old)) {
				bp += d32tostr(bp, ep - bp, arg->old);
				*bp++ = '-';
				*bp++ = '>';
			}
			if (!isnand32(arg->new)) {
				bp += d32tostr(bp, ep - bp, arg->new);
			}
		}
		*bp++ = '\n';
		*bp = '\0';
		fputs(buf, ia->f);

		arg = yield_ptr(NULL);
	}
	return 0;
}

declcoru(co_tser_flt, {
		truf_wheap_t q;
		FILE *tser;
		/* max quote age */
		echs_idiff_t mqa;
		unsigned int edgp:1U;
		unsigned int levp:1U;
	}, {});

static truf_step_cell_t
defcoru(co_tser_flt, ia, UNUSED(arg))
{
/* yields a co_edg_res when exposure changes
 * In edge mode we need to know the levels beforehand because we yield
 * upon trod changes.
 * In level mode we need to know the trod changes before the levels.
 *
 * We implement both going through trod edges first and defer their
 * yield.  We chose to defer trod edges because in practice they will
 * be sparser than price data changes. */
	static struct {
		echs_instant_t t;
		truf_step_t ref;
	} dfrd[64U];
	coru_t rdr;
	coru_t pop;
	struct truf_step_s res;

	init_coru();
	rdr = make_coru(co_echs_rdr, ia->tser);
	pop = make_coru(co_echs_pop, ia->q);

	truf_step_cell_t ev;
	const struct co_rdr_res_s *ln;
	for (ln = next(rdr), ev = next(pop); ln != NULL;) {
		size_t nemit = 0U;
		size_t ndfrd = 0U;

		/* aggregate trod directives between price lines */
		for (;
		     LIKELY(ev != NULL) &&
			     UNLIKELY(echs_instant_ge_p(ln->t, ev->t));
		     ev = next(pop)) {
			truf_sym_t sym = ev->sym;
			truf_step_t st;

			/* prep yield */
			if (!truf_mmy_abs_p(sym)) {
				sym = truf_mmy_abs(sym, ev->t.y);
			}
			/* make sure we massage the lstk */
			st = truf_step_find(sym);
			st->old = st->new;
			st->new = ev->new;

			with (echs_idiff_t age) {
				age = echs_instant_diff(ev->t, st->t);
				if (echs_idiff_ge_p(age, ia->mqa)) {
					/* max quote age exceeded */
					st->t = ev->t;
					st->bid = nand32(NULL);
					st->ask = nand32(NULL);
				}
			}
			/* defer the yield a bit,
			 * there's technically the chance that in the
			 * next few lines the price gets updated */
			dfrd[ndfrd].t = ev->t;
			dfrd[ndfrd].ref = st;
			ndfrd++;
		}

		/* yield time series lines in between trod edges */
		do {
			char *on;
			truf_sym_t sym;
			truf_step_t st;

			/* print left over deferred events,
			 * conditionalise on timestamp to maintain
			 * chronologicity */
			for (; nemit < ndfrd &&
				     echs_instant_lt_p(dfrd[nemit].t, ln->t);
			     nemit++) {
				/* just yield */
				truf_step_t ref = dfrd[nemit].ref;

				if (ref->old == ref->new) {
					continue;
				}
				if (ia->levp && !ia->edgp && ref->new == 0.df) {
					/* we already yielded this when
					 * the exposure was != 0.df */
					continue;
				}

				res = *ref;
				if (ia->edgp) {
					res.t = dfrd[nemit].t;
				}
				if (!isnand32(ref->bid)) {
					/* update exposure */
					ref->old = ref->new;
				}
				yield(res);
			}

			/* snarf symbol */
			sym = truf_mmy_rd(ln->ln, &on);

			if (!truf_mmy_abs_p(sym)) {
				sym = truf_mmy_abs(sym, ln->t.y);
			}
			/* make sure we massage the lstk */
			st = truf_step_find(sym);
			st->t = ln->t;

			/* keep track of last price */
			if (*on == '\t') {
				st->bid = strtod32(on + 1U, &on);
			}
			if (*on == '\t') {
				st->ask = strtod32(on + 1U, &on);
			}

			if (ia->levp) {
				if (st->old == st->new && st->new == 0.df) {
					/* we're not invested,
					 * and it's not an edge */
					continue;
				}
			} else if (ia->edgp && st->old == st->new) {
				/* not an edge */
				continue;
			}

			/* otherwise yield price and exposure */
			res = *st;
			/* update exposures */
			st->old = st->new;
			yield(res);
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

static truf_step_t
defcoru(co_echs_pos, ia, UNUSED(arg))
{
/* yields something that co_echs_out can use directly */
	coru_t rdr;
	coru_t pop;
	struct truf_step_s res;

	init_coru();
	if (ia->ndt == 0U) {
		/* read date/times from stdin */
		rdr = make_coru(co_echs_rdr, stdin);
	} else {
		rdr = make_coru(co_inst_rdr, ia->dt, ia->ndt);
	}
	pop = make_coru(co_echs_pop, ia->q);

	truf_step_cell_t ev;
	const struct co_rdr_res_s *ln;
	for (ln = next(rdr), ev = next(pop); ln != NULL;) {
		/* in between date/times from the RDR find trods */
		for (;
		     LIKELY(ev != NULL) &&
			     UNLIKELY(!echs_instant_lt_p(ln->t, ev->t));
		     ev = next(pop)) {
			truf_mmy_t sym = ev->sym;
			truf_step_t st;

			if (!truf_mmy_abs_p(sym)) {
				sym = truf_mmy_abs(sym, ev->t.y);
			}
			/* make sure we massage the lstk */
			st = truf_step_find(sym);
			st->old = st->new;
			st->new = ev->new;
		}

		do {
			char buf[1024U];
			char *bp = buf;
			const char *const ep = buf + sizeof(buf);

			bp += dt_strf(bp, ep - bp, ln->t);
			*bp++ = '\t';
			for (truf_step_t st; (st = truf_step_iter()) != NULL;) {
				if (st->new != 0.df) {
					/* prep the yield */
					res = *st;
					res.t = ln->t;
					res.old = nand32(NULL);
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


/* public api, might go to libtruffle one day */
static int
truf_read_trod_file(truf_wheap_t q, const char *fn)
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
		truf_add_trod(q, ln->t, c);
	}
	/* now sort the guy */
	truf_wheap_fix_deferred(q);

	free_coru(rdr);
	fini_coru();
	fclose(f);
	return 0;
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
bang_schema(truf_wheap_t q, trsch_t sch, daisy_t when)
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
			truf_add_trod(q, t, d);
		}
	}
	/* and sort the guy */
	truf_wheap_fix_deferred(q);
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
	truf_wheap_t q;
	int res = 0;

	if (argi->inputs_num < 1U) {
		fputs(usg, stderr);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	for (unsigned int i = 1U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}
	/* and print him */
	{
		coru_t pop;
		coru_t out;

		init_coru();
		pop = make_coru(co_echs_pop, q);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_given, argi->abs_given, argi->oco_given,
			.prnt_expp = true);

		for (truf_step_cell_t e; (e = next(pop)) != NULL;) {
			____next(out, e);
		}

		free_coru(pop);
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

static int
cmd_migrate(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "Usage: truffle migrate [SCHEMA-FILE]...\n";
	truf_wheap_t q;
	daisy_t from;
	daisy_t till;
	int res = 0;

	if (argi->inputs_num < 1U) {
		fputs(usg, stderr);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
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
			(void)truf_read_trod_file(q, fn);
			continue;
		}
		/* bang into wheap */
		for (daisy_t now = from; now <= till; now++) {
			bang_schema(q, sch, now);
		}
		/* and out again */
		free_schema(sch);
	}

	/* and print the whole wheap now */
	/* and print him */
	{
		coru_t pop;
		coru_t out;

		init_coru();
		pop = make_coru(co_echs_pop, q);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_given, argi->abs_given, argi->oco_given,
			.prnt_expp = true);

		for (truf_step_cell_t e; (e = next(pop)) != NULL;) {
			____next(out, e);
		}

		free_coru(pop);
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

static int
cmd_filter(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "\
Usage: truffle filter TSER-FILE [TROD-FILE]...\n";
	echs_idiff_t max_quote_age;
	truf_wheap_t q;
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usg, stderr);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	if (argi->max_quote_age_given) {
		max_quote_age = echs_idiff_rd(argi->max_quote_age_arg, NULL);
	} else {
		max_quote_age = (echs_idiff_t){4095};
	}

	for (unsigned int i = 2U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}

	with (const char *fn = argi->inputs[1U]) {
		const bool edgp = argi->edge_given;
		coru_t flt;
		coru_t out;
		FILE *f;

		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			res = 1;
			goto out;
		}

		init_coru();
		flt = make_coru(
			co_tser_flt, q, f,
			.edgp = edgp, .levp = !edgp,
			.mqa = max_quote_age);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_given, argi->abs_given, argi->oco_given,
			.prnt_prcp = true);

		for (truf_step_cell_t e; (e = next(flt)) != NULL;) {
			if (UNLIKELY(isnand32(e->bid))) {
				continue;
			}
			____next(out, e);
		}

		free_coru(flt);
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

static int
cmd_position(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "\
Usage: truffle position TROD-FILE [DATE/TIME]...\n";
	truf_wheap_t q;
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usg, stderr);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	with (const char *fn = argi->inputs[1U]) {
		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
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
			argi->rel_given, argi->abs_given, argi->oco_given,
			.prnt_expp = true);

		for (truf_step_cell_t e; (e = next(pos)) != NULL;) {
			____next(out, e);
		}

		free_coru(pos);
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

static int
cmd_glue(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "\
Usage: truffle glue TSER-FILE [TROD-FILE]...\n";
	echs_idiff_t max_quote_age;
	truf_wheap_t q;
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usg, stderr);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	if (argi->max_quote_age_given) {
		max_quote_age = echs_idiff_rd(argi->max_quote_age_arg, NULL);
	} else {
		max_quote_age = (echs_idiff_t){4095};
	}

	for (unsigned int i = 2U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}

	with (const char *fn = argi->inputs[1U]) {
		coru_t flt;
		coru_t out;
		FILE *f;

		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			res = 1;
			goto out;
		}

		init_coru();
		flt = make_coru(
			co_tser_flt, q, f,
			.edgp = true, .levp = !argi->edge_given,
			.mqa = max_quote_age);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_given, argi->abs_given, argi->oco_given,
			.prnt_expp = true, .prnt_prcp = true);

		for (truf_step_cell_t e; (e = next(flt)) != NULL;) {
			____next(out, e);
		}

		free_coru(flt);
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

static int
cmd_roll(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "\
Usage: truffle roll TSER-FILE [TROD-FILE]...\n";
	echs_idiff_t max_quote_age;
	truf_wheap_t q;
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usg, stderr);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	if (argi->max_quote_age_given) {
		max_quote_age = echs_idiff_rd(argi->max_quote_age_arg, NULL);
	} else {
		max_quote_age = (echs_idiff_t){4095};
	}

	for (unsigned int i = 2U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}

	with (const char *fn = argi->inputs[1U]) {
		truf_price_t prc = nand32(NULL);
		truf_price_t cfv = 1.df;
		coru_t flt;
		FILE *f;

		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			res = 1;
			goto out;
		}

		if (argi->basis_given) {
			prc = strtod32(argi->basis_arg, NULL);
		}
		if (argi->tick_value_given) {
			cfv = strtod32(argi->tick_value_arg, NULL);
		}

		init_coru();
		flt = make_coru(
			co_tser_flt, q, f,
			.edgp = true, .levp = !argi->edge_given,
			.mqa = max_quote_age);

		for (truf_step_cell_t e; (e = next(flt)) != NULL;) {
			char buf[256U];
			const char *const ep = buf + sizeof(buf);
			char *bp = buf;
			echs_instant_t t = e->t;
			truf_rpaf_t r = truf_rpaf_step(e);

			if (UNLIKELY(isnand32(e->bid))) {
				/* do fuckall */
				continue;
			} else if (UNLIKELY(argi->flow_given)) {
				/* flow mode means don't accrue anything */
				prc = 0.df;
			} else if (UNLIKELY(isnand32(prc))) {
				/* set initial price level to first refprc */
				prc = r.refprc;
			}

			/* sum up rpaf */
			prc += r.cruflo * cfv;

			bp += dt_strf(bp, ep - bp, t);
			*bp++ = '\t';
			bp += d32tostr(bp, ep - bp, prc);

			*bp++ = '\n';
			*bp = '\0';
			fputs(buf, stdout);
		}

		free_coru(flt);
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
	/* initialise our step and rpaf system */
	truf_init_step();
	truf_init_rpaf();

	/* check the command */
	with (const char *cmd = argi->inputs[0U]) {
		if (!strcmp(cmd, "print")) {
			res = cmd_print(argi);
		} else if (!strcmp(cmd, "roll")) {
			res = cmd_roll(argi);
		} else if (!strcmp(cmd, "migrate")) {
			res = cmd_migrate(argi);
		} else if (!strcmp(cmd, "filter")) {
			res = cmd_filter(argi);
		} else if (!strcmp(cmd, "position")) {
			res = cmd_position(argi);
		} else if (!strcmp(cmd, "glue")) {
			res = cmd_glue(argi);
		} else {
		nocmd:
			error("No valid command specified.\n\
See --help to obtain a list of available commands.");
			res = 1;
			goto out;
		}
	}

	/* finalise our step and rpaf system */
	truf_fini_step();
	truf_fini_rpaf();

out:
	/* just to make sure */
	fflush(stdout);
	truf_parser_free(argi);
	return res;
}

/* truffle.c ends here */
