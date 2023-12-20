/*** truffle.c -- tool to apply roll-over directives
 *
 * Copyright (C) 2009-2020 Sebastian Freundt
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
#if defined HAVE_DFP754_H
# include <dfp754.h>
#elif defined HAVE_DFP_STDLIB_H
# include <dfp/stdlib.h>
#else  /* !HAVE_DFP754_H && !HAVE_DFP_STDLIB_H */
extern int isinfd64(_Decimal64);
#endif	/* HAVE_DFP754_H */
#include "truffle.h"
#include "wheap.h"
#include "nifty.h"
#include "coru.h"
#include "dt-strpf.h"
#include "trod.h"
#include "step.h"
#include "rpaf.h"
/* while we're in transition mood */
#include "daisy.h"
#include "idate.h"
#include "schema.h"
#include "actcon.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:1572)
# pragma warning (disable:981)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wmissing-braces"
# pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif	/* __INTEL_COMPILER || __GNUC__ */

typedef const struct truf_step_s *truf_step_cell_t;

static int rc;


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

static bool
dxxp(const char *str, char *on[static 1U])
{
	const char *sp = str;
	bool res = true;

	/* skip sign */
	switch (*sp) {
	case '+':
	case '-':
		sp++;
	default:
		break;
	}
	/* fast forward past digits */
	for (; *sp >= '0' && *sp <= '9'; sp++);
	if (*sp <= ' ') {
		/* that's good enough, include the empty string as well */
		;
	} else if (*sp == '.') {
		sp++;
		/* more digits */
		for (; *sp >= '0' && *sp <= '9'; sp++);
		if (*sp <= ' ') {
			/* ok, job done */
			;
		} else {
			res = false;
		}
	} else {
		res = false;
	}
	/* go to the next whitespace */
	for (; *sp > ' '; sp++);
	*on = deconst(sp);
	return res;
}

static inline __attribute__((always_inline)) unsigned int
fls(unsigned int x)
{
	return x ? sizeof(x) * 8U - __builtin_clz(x) : 0U;
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


/* coroutine for the reader of echs files, key will always be date/time
 * and value is the rest of the line */
declcoru(co_echs_rdr, {
		FILE *f;
	}, {});

static const struct co_rdr_res_s {
	echs_instant_t t;
	const char *ln;
	size_t lz;
	size_t nl;
} *defcoru(co_echs_rdr, ia, UNUSED(arg))
{
	char *line = NULL;
	size_t llen = 0UL;
	ssize_t nrd;
	/* we'll yield a rdr_res */
	struct co_rdr_res_s res = {.nl = 0U};
	FILE *f = ia->f;

	while ((nrd = getline(&line, &llen, f)) > 0) {
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
		res.nl++;
		yield(res);
	}

	free(line);
	line = NULL;
	llen = 0U;
	return 0;
}

/* coroutine for the reader of quote series echs files,
 * the result is specific to truffle, returning a truf_step_cell_t */
declcoru(co_tser_rdr, {
		FILE *f;
	}, {});

static truf_step_cell_t
defcoru(co_tser_rdr, ia, UNUSED(arg))
{
	echs_instant_t olt = {.u = 0};
	coru_t rdr;
	/* we'll yield a truf_step object */
	struct truf_step_s res;
	unsigned int flds = 0U;
#define FLD_SYMBOL	(1U)
#define FLD_SETTLE	(2U)
#define FLD_BIDASK	(4U)
#define FLD_VOLUME	(8U)
#define FLD_OPNINT	(16U)

	init_coru();
	rdr = make_coru(co_echs_rdr, ia->f);

	/* get a data probe */
	const struct co_rdr_res_s *ln;
	if ((ln = next(rdr)) == NULL) {
		goto bugger;
	}
	/* try and read the first column as d32 */
	with (const char *in = ln->ln) {
		char *on;

		if (dxxp(in, &on) && on > in) {
			flds |= FLD_SETTLE;
		} else {
			/* assume symbols, even allow the empty symbol */
			flds |= FLD_SYMBOL;
		}
		if (*on == '\0' || *on == '\n') {
			break;
		}

		in = on + 1U;
		if (dxxp(in, &on) && on > in) {
			/* got more d32s,
			 * if there was no prc yet, set SETTLE,
			 * otherwise set BIDASK (and clear SETTLE) */
			flds += FLD_SETTLE;
		}
		if (*on == '\0' || *on == '\n') {
			break;
		}

		in = on + 1U;
		if (dxxp(in, &on) && on > in) {
			/* got more d32s */
			flds |= FLD_BIDASK;
		}
		if (*on == '\0' || *on == '\n') {
			break;
		}

		in = on + 1U;
		if (dxxp(in, &on) && on > in) {
			/* got more d32s */
			flds |= FLD_VOLUME;
		}
		if (*on == '\0' || *on == '\n') {
			break;
		}

		in = on + 1U;
		if (dxxp(in, &on) && on > in) {
			/* got more d32s */
			flds |= FLD_OPNINT;
		}
		if (*on == '\0' || *on == '\n') {
			break;
		}
	}

	do {
		char *on;

		res.t = ln->t;
		if (UNLIKELY(echs_instant_lt_p(res.t, olt))) {
			errno = 0, error("\
Error: violation of chronologicity in line %zu of time series", ln->nl);
			rc = -1;
			goto bugger;
		}
		olt = res.t;
		if (LIKELY(flds & FLD_SYMBOL)) {
			res.sym = truf_sym_rd(ln->ln, &on);
			on++;
		} else {
			res.sym.u = 0U;
			on = deconst(ln->ln);
		}

		/* snarf price(s) */
		if (LIKELY(flds & (FLD_SETTLE | FLD_BIDASK))) {
			res.bid = strtopx(on, &on);
		} else {
			res.bid = res.ask = NANPX;
		}

		if (UNLIKELY(flds & FLD_BIDASK)) {
			on++;
			res.ask = strtopx(on, &on);
		} else {
			res.ask = NANPX;
			on += *on == '\t';
		}

		if (UNLIKELY(flds & FLD_VOLUME)) {
			on++;
			res.vol = strtoqx(on, &on);
		} else {
			res.vol = NANQX;
			on += *on == '\t';
		}

		if (UNLIKELY(flds & FLD_OPNINT)) {
			on++;
			res.opi = strtoqx(on, &on);
		} else {
			res.opi = NANQX;
		}

		yield(res);
	} while ((ln = next(rdr)) != NULL);

bugger:
	free_coru(rdr);
	fini_coru();
	return 0;
}


declcoru(co_echs_pop, {
		truf_wheap_t q;
	}, {});

static truf_step_cell_t
defcoru(co_echs_pop, ia, UNUSED(arg))
{
/* coroutine for the wheap popper */
	/* we'll yield a pop_res */
	struct truf_step_s res = {
		.old = NANEX,
	};
	truf_wheap_t q = ia->q;

	while (!echs_instant_0_p(res.t = truf_wheap_top_rank(q))) {
		/* assume it's a truf_trod_t */
		uintptr_t tmp = truf_wheap_pop(q);
		res.sym = trods[tmp].sym[0U];
		res.new = trods[tmp].exp;
		yield(res);
		if (trods[tmp].sym[1U].u) {
			res.sym = trods[tmp].sym[1U];
			res.new = UNITEX;
			yield(res);
		}
	}
	return 0;
}

declcoru(co_inst_rdr, {
		const char *const *dt;
		size_t ndt;
	}, {});

static const struct co_rdr_res_s*
defcoru(co_inst_rdr, ia, UNUSED(arg))
{
/* coroutine for the reader of the tseries */
	const char *const *dt = ia->dt;
	const char *const *const edt = ia->dt + ia->ndt;
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
_defcoru(co_echs_out, iap, truf_step_cell_t arg)
{
	char buf[256U];
	const char *const ep = buf + sizeof(buf);
	coru_initargs(co_echs_out) ia = *iap;

	while (arg != NULL) {
		char *bp = buf;
		echs_instant_t t = arg->t;
		truf_sym_t sym = arg->sym;

		bp += dt_strf(bp, ep - bp, t);
		if (LIKELY(sym.u)) {
			/* convert mmys */
			if (truf_mmy_p(sym)) {
				if (ia.ocop) {
					sym.mmy = truf_mmy_oco(sym.mmy, t.y);
				} else if (ia.absp) {
					sym.mmy = truf_mmy_abs(sym.mmy, t.y);
				} else if (ia.relp) {
					sym.mmy = truf_mmy_rel(sym.mmy, t.y);
				}
			}
			*bp++ = '\t';
			bp += truf_sym_wr(bp, ep - bp, sym);
		}
		if (ia.prnt_prcp) {
			if (!isnanpx(arg->bid)) {
				*bp++ = '\t';
				bp += pxtostr(bp, ep - bp, arg->bid);
			}
			if (!isnanpx(arg->ask)) {
				*bp++ = '\t';
				bp += pxtostr(bp, ep - bp, arg->ask);
			}
		}
		if (ia.prnt_expp) {
			*bp++ = '\t';
			if (!isnanpx(arg->old)) {
				bp += extostr(bp, ep - bp, arg->old);
				*bp++ = '-';
				*bp++ = '>';
			}
			if (!isnanpx(arg->new)) {
				bp += extostr(bp, ep - bp, arg->new);
			}
		}
		*bp++ = '\n';
		*bp = '\0';
		fputs(buf, ia.f);

		arg = yield_ptr(NULL);
	}
	return 0;
}

declcoru(co_roll_out, {
		FILE *f;
		bool absp;
		signed int prec;
	}, {
		echs_instant_t t;
		truf_price_t prc;
		truf_quant_t vol;
		truf_quant_t opi;
	});

static const void*
defcoru(co_roll_out, iap, arg)
{
	char buf[256U];
	const char *const ep = buf + sizeof(buf);
	coru_initargs(co_roll_out) ia = *iap;

	if (!ia.absp) {
		/* non-abs precision mode */
		while (arg != NULL) {
			char *bp = buf;
			truf_price_t prc;

			if (UNLIKELY(isnanpx(prc = arg->prc))) {
				/* refuse to print nans */
				goto nex2;
			}

			/* print time stamp */
			bp += dt_strf(bp, ep - bp, arg->t);
			*bp++ = '\t';

			/* scale to precision */
			if (UNLIKELY(ia.prec)) {
				/* come up with a new raw value */
				int tgtx = quantexpd(prc) + ia.prec;
				truf_price_t scal = scalbnd(ZEROPX, tgtx);
				prc = quantized(prc, scal);
			}
			bp += pxtostr(bp, ep - bp, prc);
			if (!isnanqx(arg->vol)) {
				*bp++ = '\t';
				bp += qxtostr(bp, ep - bp, arg->vol);
				if (!isnanqx(arg->opi)) {
					*bp++ = '\t';
					bp += qxtostr(bp, ep - bp, arg->opi);
				}
			}
			*bp++ = '\n';
			*bp = '\0';
			fputs(buf, ia.f);
		nex2:
			arg = yield_ptr(NULL);
		}
	} else /*if (absp)*/ {
		/* produce a price with -ia.prec fractional digits */
		const truf_price_t scal = scalbnd(UNITPX, ia.prec);

		/* absolute precision mode */
		while (arg != NULL) {
			char *bp = buf;
			truf_price_t prc;

			if (UNLIKELY(isnanpx(prc = arg->prc))) {
				/* refuse to print nans */
				goto nex4;
			}
			/* print time stamp */
			bp += dt_strf(bp, ep - bp, arg->t);
			*bp++ = '\t';

			/* scale to precision */
			prc = quantized(prc, scal);
			bp += pxtostr(bp, ep - bp, prc);
			*bp++ = '\n';
			*bp = '\0';
			fputs(buf, ia.f);
		nex4:
			arg = yield_ptr(NULL);
		}
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
defcoru(co_tser_flt, iap, UNUSED(arg))
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
	} _dfrd[64U], *dfrd = _dfrd;
	coru_t rdr;
	coru_t pop;
	coru_initargs(co_tser_flt) ia = *iap;
	struct truf_step_s res;

	init_coru();
	rdr = make_coru(co_tser_rdr, ia.tser);
	pop = make_coru(co_echs_pop, ia.q);

	truf_step_cell_t ev;
	truf_step_cell_t qu;
	for (qu = next(rdr), ev = next(pop); qu != NULL;) {
		size_t nemit = 0U;
		size_t ndfrd = 0U;

		/* aggregate trod directives between price lines */
		for (;
		     LIKELY(ev != NULL) &&
			     UNLIKELY(echs_instant_ge_p(qu->t, ev->t));
		     ev = next(pop)) {
			truf_sym_t sym = ev->sym;
			truf_step_t st;

			/* prep yield */
			if (!truf_mmy_p(sym)) {
				/* transform not */
				;
			} else if (!truf_mmy_abs_p(sym.mmy)) {
				sym.mmy = truf_mmy_abs(sym.mmy, ev->t.y);
			}
			/* make sure we massage the lstk */
			st = truf_step_find(sym);
			st->old = st->new;
			st->new = ev->new;

			with (echs_idiff_t age) {
				age = echs_instant_diff(ev->t, st->t);
				if (echs_idiff_ge_p(age, ia.mqa)) {
					/* max quote age exceeded */
					st->t = ev->t;
					st->bid = NANPX;
					st->ask = NANPX;
				}
			}
			/* defer the yield a bit,
			 * there's technically the chance that in the
			 * next few lines the price gets updated */
			dfrd[ndfrd].t = ev->t;
			dfrd[ndfrd].ref = st;
			ndfrd++;
			/* check that there's room for the next deferral */
			if (UNLIKELY(ndfrd == countof(_dfrd))) {
				/* relocate to heap space aye */
				dfrd = malloc(2 * sizeof(_dfrd));
				memcpy(dfrd, _dfrd, sizeof(_dfrd));
			} else if (UNLIKELY(_dfrd != dfrd) &&
				   UNLIKELY(fls(ndfrd) > fls(ndfrd - 1U))) {
				/* new 2 power, double in size */
				dfrd = realloc(dfrd, 2 * ndfrd * sizeof(*dfrd));
			}
		}

		/* yield time series lines in between trod edges */
		do {
			truf_sym_t sym = qu->sym;
			truf_step_t st;

			/* print left over deferred events,
			 * conditionalise on timestamp to maintain
			 * chronologicity */
			for (; nemit < ndfrd &&
				     echs_instant_lt_p(dfrd[nemit].t, qu->t);
			     nemit++) {
				/* just yield */
				truf_step_t ref = dfrd[nemit].ref;

				if (ref->old == ref->new) {
					continue;
				}
				if (ia.levp && !ia.edgp && ref->new == ZEROEX) {
					/* we already yielded this when
					 * the exposure was != 0.df */
					continue;
				}

				res = *ref;
				if (ia.edgp) {
					res.t = dfrd[nemit].t;
				}
				if (!isnanpx(ref->bid)) {
					/* update exposure */
					ref->old = ref->new;
				}
				yield(res);
			}

			/* snarf symbol, always abs(?) */
			if (!truf_mmy_p(sym)) {
				/* transform not */
				;
			} else if (!truf_mmy_abs_p(sym.mmy)) {
				sym.mmy = truf_mmy_abs(sym.mmy, qu->t.y);
			}
			/* make sure we massage the lstk */
			st = truf_step_find(sym);
			st->t = qu->t;

			/* keep track of last price */
			st->bid = qu->bid;
			st->ask = qu->ask;
			st->vol = qu->vol;
			st->opi = qu->opi;

			if (ia.levp) {
				if (st->old == st->new && st->new == ZEROEX) {
					/* we're not invested,
					 * and it's not an edge */
					continue;
				}
			} else if (ia.edgp && st->old == st->new) {
				/* not an edge */
				continue;
			}

			/* otherwise yield price and exposure */
			res = *st;
			/* update exposures */
			st->old = st->new;
			yield(res);
		} while (LIKELY((qu = next(rdr)) != NULL) &&
			 (UNLIKELY(ev == NULL) ||
			  LIKELY(echs_instant_lt_p(qu->t, ev->t))));
	}

	if (UNLIKELY(_dfrd != dfrd)) {
		/* reset the deferred states */
		free(dfrd);
		dfrd = _dfrd;
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
			truf_sym_t sym = ev->sym;
			truf_step_t st;

			if (!truf_mmy_p(sym)) {
				/* transform not */
				;
			} else if (!truf_mmy_abs_p(sym.mmy)) {
				sym.mmy = truf_mmy_abs(sym.mmy, ev->t.y);
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
				if (st->new != ZEROEX) {
					/* prep the yield */
					res = *st;
					res.t = ln->t;
					res.old = NANEX;
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

static const truf_trod_t truf_nul_trod = {.exp = ZEROEX};

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
				res.exp = ZEROEX;
			} else if (n2->y != 0.0 && n1->y == 0.0) {
				res.exp = UNITEX;
			} else {
				continue;
			}
		} else if (j == 0 && when == l1) {
			/* something happened at l1 */
			if (UNLIKELY(n1->y != 0.0)) {
				res.exp = UNITEX;
			} else {
				continue;
			}
		} else {
			continue;
		}
		res.sym[0U].mmy = make_truf_mmy(
			y + p->year_off, m_to_i(p->month), 0U);
		res.sym[1U].u = 0U;

		/* indicate success (as in clear for adding) */
		return res;
	}
	/* indicate failure (to add anything) */
	return truf_nul_trod;
}

static void
bang_schema(truf_wheap_t q, trsch_t sch, daisy_t when, unsigned int lax)
{
	echs_instant_t t = daisy_to_instant(when);

	for (size_t i = 0; i < sch->np; i++) {
		const struct cline_s *p = sch->p[i];
		truf_trod_t d;

		/* check year validity */
		if (when < p->valid_from || when > p->valid_till + lax) {
			/* cline isn't applicable */
			;
		} else if (!((d = make_trod_from_cline(p, when)).sym[0U].u)) {
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


#include "truffle.yucc"

static int
cmd_print(const struct yuck_cmd_print_s argi[static 1U])
{
	truf_wheap_t q;

	if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		rc = -1;
		goto out;
	}

	for (size_t i = 0U; i < argi->nargs + !argi->nargs; i++) {
		const char *fn = argi->args[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			rc = -1;
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
			argi->rel_flag, argi->abs_flag, argi->oco_flag,
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
	return rc < 0;
}

static int
cmd_migrate(const struct yuck_cmd_migrate_s argi[static 1U])
{
	truf_wheap_t q;
	daisy_t from;
	daisy_t till;
	unsigned int lax = 0U;

	if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		rc = -1;
		goto out;
	}

	if (argi->from_arg) {
		echs_instant_t i = dt_strp(argi->from_arg, NULL);
		from = instant_to_daisy(i);
	} else {
		echs_instant_t i = {2000U, 1U, 1U, ECHS_ALL_DAY};
		from = instant_to_daisy(i);
	}
	if (argi->till_arg) {
		echs_instant_t i = dt_strp(argi->till_arg, NULL);
		till = instant_to_daisy(i);
	} else {
		echs_instant_t i = {2037U, 12U, 31U, ECHS_ALL_DAY};
		till = instant_to_daisy(i);
	}
	if (argi->lax_arg) {
		lax = strtoul(argi->lax_arg, NULL, 0);
	}

	for (size_t i = 0U; i < argi->nargs; i++) {
		const char *fn = argi->args[i];
		trsch_t sch;

		if ((sch = read_schema(fn)) == NULL) {
			/* try the normal trod reader */
			(void)truf_read_trod_file(q, fn);
			continue;
		}
		/* bang into wheap */
		for (daisy_t now = from; now <= till; now++) {
			bang_schema(q, sch, now, lax);
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
			argi->rel_flag, argi->abs_flag, argi->oco_flag,
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
	return rc < 0;
}

static int
cmd_filter(const struct yuck_cmd_filter_s argi[static 1U])
{
	echs_idiff_t max_quote_age;
	truf_wheap_t q;

	if (argi->nargs < 1U) {
		yuck_auto_usage((const yuck_t*)argi);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		rc = -1;
		goto out;
	}

	if (argi->max_quote_age_arg) {
		max_quote_age = echs_idiff_rd(argi->max_quote_age_arg, NULL);
	} else {
		max_quote_age = (echs_idiff_t){4095};
	}

	for (size_t i = 1U; i < argi->nargs + (argi->nargs <= 1U); i++) {
		const char *fn = argi->args[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			rc = -1;
			goto out;
		}
	}

	with (const char *fn = *argi->args) {
		const bool edgp = argi->edge_flag;
		coru_t flt;
		coru_t out;
		FILE *f;

		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			rc = -1;
			goto out;
		}

		init_coru();
		flt = make_coru(
			co_tser_flt, q, f,
			.edgp = edgp, .levp = !edgp,
			.mqa = max_quote_age);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_flag, argi->abs_flag, argi->oco_flag,
			.prnt_prcp = true);

		for (truf_step_cell_t e; (e = next(flt)) != NULL;) {
			if (UNLIKELY(isnanpx(e->bid))) {
				continue;
			}
			____next(out, e);
		}

		free_coru(flt);
		free_coru(out);
		fini_coru();
		fclose(f);
	}
out:
	if (LIKELY(q != NULL)) {
		free_truf_wheap(q);
	}
	truf_free_trods();
	return rc < 0;
}

static int
cmd_position(const struct yuck_cmd_position_s argi[static 1U])
{
	truf_wheap_t q;

	if (argi->nargs < 1U) {
		yuck_auto_usage((const yuck_t*)argi);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		rc = -1;
		goto out;
	}

	with (const char *fn = argi->args[0U]) {
		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			rc = -1;
			goto out;
		}
	}

	/* just print them now */
	{
		char *const *dt = argi->args + 1U;
		const size_t ndt = argi->nargs - 1U;
		coru_t pos;
		coru_t out;

		init_coru();
		pos = make_coru(co_echs_pos, q, (const char*const*)dt, ndt);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_flag, argi->abs_flag, argi->oco_flag,
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
	return rc < 0;
}

static int
cmd_glue(const struct yuck_cmd_glue_s argi[static 1U])
{
	echs_idiff_t max_quote_age;
	truf_wheap_t q;

	if (argi->nargs < 1U) {
		yuck_auto_usage((const yuck_t*)argi);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		rc = -1;
		goto out;
	}

	if (argi->max_quote_age_arg) {
		max_quote_age = echs_idiff_rd(argi->max_quote_age_arg, NULL);
	} else {
		max_quote_age = (echs_idiff_t){4095};
	}

	for (unsigned int i = 1U; i < argi->nargs; i++) {
		const char *fn = argi->args[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			rc = -1;
			goto out;
		}
	}

	with (const char *fn = argi->args[0U]) {
		coru_t flt;
		coru_t out;
		FILE *f;

		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			rc = -1;
			goto out;
		}

		init_coru();
		flt = make_coru(
			co_tser_flt, q, f,
			.edgp = true, .levp = !argi->edge_flag,
			.mqa = max_quote_age);
		out = make_coru(
			co_echs_out, stdout,
			argi->rel_flag, argi->abs_flag, argi->oco_flag,
			.prnt_expp = true, .prnt_prcp = true);

		for (truf_step_cell_t e; (e = next(flt)) != NULL;) {
			____next(out, e);
		}

		free_coru(flt);
		free_coru(out);
		fini_coru();
		fclose(f);
	}
out:
	if (LIKELY(q != NULL)) {
		free_truf_wheap(q);
	}
	truf_free_trods();
	return rc < 0;
}

static int
cmd_roll(const struct yuck_cmd_roll_s argi[static 1U])
{
	echs_idiff_t max_quote_age;
	truf_wheap_t q;

	if (argi->nargs < 1U) {
		yuck_auto_usage((const yuck_t*)argi);
		return 1;
	} else if (UNLIKELY((q = make_truf_wheap()) == NULL)) {
		rc = -1;
		goto out;
	}

	if (argi->max_quote_age_arg) {
		max_quote_age = echs_idiff_rd(argi->max_quote_age_arg, NULL);
	} else {
		max_quote_age = (echs_idiff_t){4095};
	}

	for (unsigned int i = 1U; i < argi->nargs + (argi->nargs <= 1U); i++) {
		const char *fn = argi->args[i];

		if (UNLIKELY(truf_read_trod_file(q, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			rc = -1;
			goto out;
		}
	}

	with (const char *fn = argi->args[0U]) {
		truf_price_t prc = NANPX;
		truf_price_t cfv = 1.df;
		bool abs_prec_p = false;
		signed int prec = 0;
		coru_t flt;
		coru_t out;
		FILE *f;
		const char *p;

		if ((p = argi->precision_arg)) {
			char *on;

			if (*p == '+' || *p == '-') {
				;
			} else {
				abs_prec_p = true;
			}
			if ((prec = -strtol(p, &on, 10), *on)) {
				error("invalid precision `%s'",
				      argi->precision_arg);
				rc = -1;
				goto out;
			}
		}
		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			rc = -1;
			goto out;
		}

		if (argi->basis_arg) {
			prc = strtopx(argi->basis_arg, NULL);
		}
		if (argi->tick_value_arg) {
			cfv = strtopx(argi->tick_value_arg, NULL);
		}

		init_coru();
		flt = make_coru(
			co_tser_flt, q, f,
			.edgp = true, .levp = !argi->edge_flag,
			.mqa = max_quote_age);
		out = make_coru(
			co_roll_out, stdout,
			.absp = abs_prec_p, .prec = prec);

		echs_instant_t metro = {9999U};
		coru_args(co_roll_out) oa = {};
		for (truf_step_cell_t e; (e = next(flt)) != NULL;) {
			echs_instant_t t = e->t;
			truf_rpaf_t r = truf_rpaf_step(e);

			if (UNLIKELY(isnanpx(e->bid))) {
				/* do fuckall */
				continue;
			} else if (UNLIKELY(isnanpx(prc))) {
				/* set initial price level to first refprc */
				signed int iqu = 0;

				prc = r.refprc;
				/* get the quantum right for this one */
				iqu += quantexpd(prc);
				iqu += quantexpd(r.cruflo);
				iqu += quantexpd(cfv);
				prc = quantized(prc, scalbnd(ZEROPX, iqu));
			} else {
				/* sum up rpaf */
				prc += r.cruflo * cfv;
			}

			/* defer by one, to avoid time dupes */
			if (echs_instant_lt_p(metro, t)) {
				next_with(out, oa);
			}
			oa = pack_args(co_roll_out, t, prc, r.cruvol, r.cruopi);
			metro = t;
		}
		/* drain */
		if (!isnanpx(prc) && rc >= 0) {
			next_with(out, oa);
		}

		free_coru(flt);
		free_coru(out);
		fini_coru();
		fclose(f);
	}
out:
	if (LIKELY(q != NULL)) {
		free_truf_wheap(q);
	}
	truf_free_trods();
	return rc < 0;
}

static int
cmd_flow(const struct yuck_cmd_flow_s argi[static 1U])
{
	FILE *f;
	coru_t rdr;
	coru_t out;

	if (argi->nargs > 1U) {
		yuck_auto_usage((const yuck_t*)argi);
		return 1;
	}

	if (!argi->nargs) {
		/* just keep stdin then */
		f = stdin;
	} else with (const char *fn = argi->args[0U]) {
		if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
			error("cannot open time series file `%s'", fn);
			rc = -1;
			goto out;
		}
	}

	init_coru();
	rdr = make_coru(co_tser_rdr, f);
	out = make_coru(
		co_echs_out, stdout,
		argi->rel_flag, argi->abs_flag, argi->oco_flag,
		.prnt_expp = false, .prnt_prcp = true);

	for (truf_step_cell_t e; (e = next(rdr)) != NULL;) {
		/* find e's sym in the step cache */
		truf_step_t st = truf_step_find(e->sym);
		struct truf_step_s fe = *e;

		if (UNLIKELY(isnanpx(st->bid))) {
			st->bid = e->bid;
		}
		if (UNLIKELY(isnanpx(st->ask))) {
			st->ask = e->ask;
		}
		/* fiddle with the local step cell to make it cash flows */
		fe.bid -= st->bid;
		fe.ask -= st->ask;
		/* store last version in step[tm] */
		*st = *e;
		next_with(out, fe);
	}

	free_coru(rdr);
	free_coru(out);
	fini_coru();
	fclose(f);

out:
	return rc < 0;
}

static int
cmd_expcon(const struct yuck_cmd_expcon_s argi[static 1U])
{
	struct actcon_s *spec;
	char from, till;

	if (UNLIKELY(argi->nargs < 1U)) {
		yuck_auto_usage((const yuck_t*)argi);
		return 1;
	}

	if (UNLIKELY((spec = read_actcon(argi->args[0U])) == NULL)) {
		return 1;
	}

#if 0
	prnt_actcon(spec);
#endif
	if (argi->nargs < 2U) {
		from = '@';
		till = (char)(!argi->yes_flag ? 'Z' : '~');
	} else {
		from = till = *argi->args[1U];
	}
	if (!argi->longest_flag) {
		xpnd_actcon(spec, from, till);
	} else {
		long_actcon(spec, from, till);
	}

	free_actcon(spec);
	return 0;
}

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int res = 0;

	if (yuck_parse(argi, argc, argv)) {
		res = 1;
		goto out;
	}

	/* get the coroutines going */
	init_coru_core();
	/* initialise our step and rpaf system */
	truf_init_sym();
	truf_init_step();
	truf_init_rpaf();

	/* check the command */
	switch (argi->cmd) {
	default:
	case TRUFFLE_CMD_NONE:
		error("No valid command specified.\n\
See --help to obtain a list of available commands.");
		res = 1;
		break;
	case TRUFFLE_CMD_PRINT:
		res = cmd_print((const void*)argi);
		break;
	case TRUFFLE_CMD_ROLL:
		res = cmd_roll((const void*)argi);
		break;
	case TRUFFLE_CMD_MIGRATE:
		res = cmd_migrate((const void*)argi);
		break;
	case TRUFFLE_CMD_FILTER:
		res = cmd_filter((const void*)argi);
		break;
	case TRUFFLE_CMD_POSITION:
		res = cmd_position((const void*)argi);
		break;
	case TRUFFLE_CMD_GLUE:
		res = cmd_glue((const void*)argi);
		break;
	case TRUFFLE_CMD_FLOW:
		res = cmd_flow((const void*)argi);
		break;
	case TRUFFLE_CMD_EXPCON:
		res = cmd_expcon((const void*)argi);
		break;
	}

	/* finalise our step and rpaf system */
	truf_fini_step();
	truf_fini_rpaf();
	truf_fini_sym();

out:
	/* just to make sure */
	fflush(stdout);
	fflush(stderr);
	yuck_free(argi);
	return res;
}

/* truffle.c ends here */
