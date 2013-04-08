/*** truffle.c -- tool to roll-over futures contracts
 *
 * Copyright (C) 2011-2013 Sebastian Freundt
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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/mman.h>
#if defined WORDS_BIGENDIAN
# include <limits.h>
#endif	/* WORDS_BIGENDIAN */
#include "truffle.h"
#include "schema.h"
#include "cut.h"
#include "yd.h"
#include "dt-strpf.h"

#if defined STANDALONE
# include <stdio.h>
#endif	/* STANDALONE */
#if !defined __GNUC__ && !defined __INTEL_COMPILER
# define __builtin_expect(x, y)	x
#endif	/* !GCC && !ICC */
#if defined __INTEL_COMPILER
# pragma warning (disable:1572)
#endif	/* __INTEL_COMPILER */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */
#if defined DEBUG_FLAG
# define TRUF_DEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define TRUF_DEBUG(args...)
#endif	/* DEBUG_FLAG */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*(x)))
#endif	/* !countof */

typedef struct trser_s *trser_t;
typedef struct trtsc_s *trtsc_t;
typedef const struct trtsc_s *const_trtsc_t;

struct __dv_s {
	idate_t d;
	double v;
};

struct __dvv_s {
	idate_t d;
	daysi_t dd;
	double *v;
};

struct trtsc_s {
	size_t ndvvs;
	size_t ncons;
	idate_t first;
	idate_t last;
	uint32_t *cons;
	struct __dvv_s *dvvs;
};
#define TSC_STEP	(4096)
#define CYM_STEP	(256)


/* alloc helpers */
#define PROT_MEM		(PROT_READ | PROT_WRITE)
#define MAP_MEM			(MAP_PRIVATE | MAP_ANONYMOUS)

static inline int
resize_mmap(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	if (cnt == 0) {
		size_t new = inc * blksz;
		*ptr = mmap(NULL, new, PROT_MEM, MAP_MEM, 0, 0);
		return 1;

	} else if (cnt % inc == 0) {
		/* resize */
		size_t old = cnt * blksz;
		size_t new = (cnt + inc) * blksz;
		*ptr = mremap(*ptr, old, new, MREMAP_MAYMOVE);
		return 1;
	}
	return 0;
}

static inline void
unsize_mmap(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	size_t sz;

	if (*ptr == NULL || cnt == 0) {
		return;
	}

	sz = ((cnt - 1) / inc + 1) * inc * blksz;
	munmap(*ptr, sz);
	*ptr = NULL;
	return;
}

static inline void
upsize_mmap(void **ptr, size_t cnt, size_t cnn, size_t blksz, size_t inc)
{
/* like resize, but definitely provide space for cnt + inc objects */
	size_t old = (cnt / inc + 1) * inc * blksz;
	size_t new = (cnn / inc + 1) * inc * blksz;

	if (*ptr == NULL) {
		*ptr = mmap(NULL, new, PROT_MEM, MAP_MEM, 0, 0);
	} else if (old < new) {
		*ptr = mremap(*ptr, old, new, MREMAP_MAYMOVE);
	}
	return;
}

static inline int
resize_mall(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	if (cnt == 0) {
		*ptr = calloc(inc, blksz);
		return 1;
	} else if (cnt % inc == 0) {
		/* resize */
		size_t new = (cnt + inc) * blksz;
		*ptr = realloc(*ptr, new);
		return 1;
	}
	return 0;
}

static inline void
unsize_mall(void **ptr, size_t cnt, size_t UNUSED(blksz), size_t UNUSED(inc))
{
	if (*ptr == NULL || cnt == 0) {
		return;
	}

	free(*ptr);
	*ptr = NULL;
	return;
}

static inline void
upsize_mall(void **ptr, size_t cnt, size_t cnn, size_t blksz, size_t inc, int f)
{
/* like resize, but definitely provide space for cnt + inc objects */
	size_t old = (cnt / inc + 1) * inc * blksz;
	size_t new = (cnn / inc + 1) * inc * blksz;
	if (*ptr == NULL) {
		*ptr = malloc(new);
		memset(*ptr, f, new);
	} else if (old < new) {
		*ptr = realloc(*ptr, new);
		memset((char*)*ptr + old, f, new - old);
	}
	return;
}


#if 0
DEFUN bool
cut_non_nil_expo_p(trcut_t cut)
{
/* return whether a cut has a non-nil exposure,
 * note it is possible for a cut to have total exposure 0.0 and
 * yet have cash flows */
	for (size_t i = 0; i < cut->ncomps; i++) {
		if (cut->comps[i].y != 0.0) {
			return true;
		}
	}
	return false;
}
#endif


/* series handling */
static trym_t
cym_to_ym(char month, unsigned int year)
{
	return ((uint16_t)year << 8U) + (uint8_t)month;
}

static ssize_t
tsc_find_cym_idx(const_trtsc_t s, trym_t ym)
{
	for (size_t i = 0; i < s->ncons; i++) {
		if (s->cons[i] == ym) {
			return i;
		}
	}
	return -1;
}

static struct __dvv_s*
tsc_find_dvv(trtsc_t s, idate_t dt)
{
	for (size_t i = 0; i < s->ndvvs; i++) {
		if (s->dvvs[i].d == dt) {
			return s->dvvs + i;
		}
	}
	return NULL;
}

static ssize_t
tsc_find_dvv_idx(trtsc_t s, idate_t dt)
{
/* find first index where dvv date >= dt */
	for (size_t i = 0; i < s->ndvvs; i++) {
		if (s->dvvs[i].d >= dt) {
			return i;
		}
	}
	return -1;
}

static void
tsc_move(trtsc_t s, ssize_t idx, int num)
{
/* move dvv vector from index IDX onwards so that NUM dvvs fit in-between */
	void **tmp = (void**)&s->dvvs;
	size_t nmov;
	size_t nndvvs = s->ndvvs + num;

	upsize_mmap(tmp, s->ndvvs, nndvvs, sizeof(*s->dvvs), TSC_STEP);
	nmov = (s->ndvvs - idx) * sizeof(*s->dvvs);
	memmove(s->dvvs + idx + num, s->dvvs + idx, nmov);
	memset(s->dvvs + idx, 0, num * sizeof(*s->dvvs));
	s->ndvvs = nndvvs;
	return;
}

static struct __dvv_s*
tsc_init_dvv(trtsc_t s, size_t idx, idate_t dt)
{
	struct __dvv_s *t = s->dvvs + idx;
	t->d = dt;
	/* make room for s->ncons doubles and set them to nan */
	upsize_mall((void**)&t->v, 0, s->ncons, sizeof(*t->v), CYM_STEP, -1);
	return t;
}

static void
tsc_add_dv(trtsc_t s, char mon, unsigned int yoff, struct __dv_s dv)
{
	struct __dvv_s *this = NULL;
	ssize_t idx = 0;

	/* find the date in question first */
	if (dv.d > s->last) {
		/* append */
		void **tmp = (void**)&s->dvvs;
		resize_mmap(tmp, s->ndvvs, sizeof(*s->dvvs), TSC_STEP);
		this = tsc_init_dvv(s, s->ndvvs++, dv.d);
		/* update stats */
		s->last = dv.d;
		if (UNLIKELY(s->first == 0)) {
			s->first = dv.d;
		}
	} else if ((this = tsc_find_dvv(s, dv.d)) != NULL) {
		/* bingo */
		;
	} else if (dv.d < s->first ||
		   (idx = tsc_find_dvv_idx(s, dv.d)) > 0) {
		/* prepend, FUCK */
		tsc_move(s, idx, 1);
		this = tsc_init_dvv(s, idx, dv.d);
		if (UNLIKELY(dv.d < s->first)) {
			s->first = dv.d;
		}
		fputs("\
warning: unsorted input data will result in poor performance\n", stderr);
	} else {
		abort();
	}

	/* now find the cmy offset */
	if ((idx = tsc_find_cym_idx(s, cym_to_ym(mon, yoff))) < 0) {
		/* append symbol */
		void **tmp = (void**)&s->cons;
		if (resize_mall(tmp, s->ncons, sizeof(*s->cons), CYM_STEP)) {
			for (size_t i = 0; i < s->ndvvs; i++) {
				upsize_mall(
					(void**)&s->dvvs[i].v, 0, s->ncons,
					sizeof(*s->dvvs[i].v), CYM_STEP, -1);
			}
		}
		idx = s->ncons++;
		s->cons[idx] = cym_to_ym(mon, yoff);
	}
	/* resize the double vector maybe */
	this->v[idx] = dv.v;
	return;
}

static trtsc_t
read_series(FILE *f)
{
	trtsc_t res = NULL;
	size_t llen = 0UL;
	char *line = NULL;

	/* get us some container */
	res = calloc(1, sizeof(*res));
	/* read the series file first */
	while (getline(&line, &llen, f) > 0) {
		char *con = line;
		char *dat;
		char *val;
		char mon;
		unsigned int yoff;
		struct __dv_s dv;

		if ((dat = strchr(con, '\t')) == NULL) {
			break;
		}
		if (!(dv.d = read_date(dat + 1, &val)) ||
		    (val == NULL) ||
		    (dv.v = strtod(val + 1, &val), val) == NULL) {
			break;
		}

		mon = con[0];
		yoff = strtoul(con + 1, NULL, 10);
		tsc_add_dv(res, mon, yoff, dv);
	}
	if (line) {
		free(line);
	}
	return res;
}

static void
free_series(trtsc_t s)
{
	for (size_t i = 0; i < s->ndvvs; i++) {
		void **tmp = (void**)&s->dvvs[i].v;
		unsize_mall(tmp, s->ncons, sizeof(double), CYM_STEP);
	}
	unsize_mall((void**)&s->cons, s->ncons, sizeof(*s->cons), CYM_STEP);
	unsize_mmap((void**)&s->dvvs, s->ndvvs, sizeof(*s->dvvs), TSC_STEP);
	free(s);
	return;
}

typedef enum {
	CUTFLO_TRANS_NIL_NIL = 0,
	CUTFLO_TRANS_NON_NIL,
	CUTFLO_TRANS_NIL_NON,
	CUTFLO_TRANS_NON_NON,
	CUTFLO_HAS_TRANS_BIT = 4,
} cutflo_trans_t;

struct __cutflo_st_s {
	/* user settable */
	/** tick value by which to multiply the cash flow */
	double tick_val;
	/**
	 * basis, unused unless set to nan in which case cut flow will
	 * pick a suitable basis, most of the time the first quote found */
	double basis;
	const_trtsc_t tsc;

	/* our stuff */
	union {
		cutflo_trans_t e;
		struct {
#if defined WORDS_BIGENDIAN
			unsigned int:sizeof(unsigned int) * CHAR_BIT - 3;
			unsigned int has_trans:1;
			unsigned int is_non_nil:1;
			unsigned int was_non_nil:1;
#else  /* !WORDS_BIGENDIAN */
			unsigned int was_non_nil:1;
			unsigned int is_non_nil:1;
			unsigned int has_trans:1;
#endif	/* WORDS_BIGENDIAN */
		};
	};
	/* should align neatly with the previous on 64b systems */
	unsigned int dvv_idx;

	double *bases;
	double *expos;
	double cum_flo;
	double inc_flo;
};

static void
init_cutflo_st(
	struct __cutflo_st_s *st, const_trtsc_t series,
	double tick_val, double basis)
{
	st->tick_val = tick_val;
	st->basis = basis;
	st->tsc = series;
	st->e = CUTFLO_TRANS_NIL_NIL;
	st->dvv_idx = 0;
	st->bases = calloc(series->ncons, sizeof(*st->bases));
	st->expos = calloc(series->ncons, sizeof(*st->expos));
	st->cum_flo = 0.0;
	st->inc_flo = 0.0;
	return;
}

static void
free_cutflo_st(struct __cutflo_st_s *st)
{
	free(st->bases);
	free(st->expos);
	return;
}

static void
warn_noquo(idate_t dt, char mon, unsigned int year, double expo)
{
	char dts[32];
	snprint_idate(dts, sizeof(dts), dt);
	fprintf(stderr, "\
cut as of %s contained %c%u with an exposure of %.8g but no quotes\n",
		dts, mon, year, expo);
	return;
}

static cutflo_trans_t
cut_flow(struct __cutflo_st_s *st, trcut_t c, idate_t dt)
{
	double res = 0.0;
	const double *new_v = NULL;
	int is_non_nil = 0;

	for (size_t i = st->dvv_idx; i < st->tsc->ndvvs; i++) {
		/* assume sortedness */
		if (st->tsc->dvvs[i].d > dt) {
			break;
		} else if (st->tsc->dvvs[i].d == dt) {
			new_v = st->tsc->dvvs[i].v;
			st->dvv_idx = i + 1;
			break;
		}
	}
	for (size_t i = 0; i < c->ncomps; i++) {
		char mon = c->comps[i].month;
		uint16_t year = c->comps[i].year;
		trym_t ym = cym_to_ym(mon, year);
		double expo;
		ssize_t idx;
		double flo;

		if (ym == 0) {
			continue;
		}
		expo = c->comps[i].y * st->tick_val;

		if ((idx = tsc_find_cym_idx(st->tsc, ym)) < 0 ||
		    isnan(new_v[idx])) {
			if (expo != 0.0) {
				warn_noquo(dt, mon, year, expo);
			} else {
				cut_rem_cc(c, c->comps + i);
			}
			continue;
		}
		/* check for transition changes */
		if (st->expos[idx] != expo) {
			if (st->expos[idx] != 0.0) {
				double tot_flo = new_v[idx] - st->bases[idx];
				flo = tot_flo * st->expos[idx];
			} else {
				flo = 0.0;
				/* guess a basis if the user asked us to */
				if (isnan(st->basis)) {
					st->basis = new_v[idx];
				}
			}

			TRUF_DEBUG("TR %+.8g @ %.8g -> %+.8g @ %.8g -> %.8g\n",
				   expo - st->expos[idx], new_v[idx],
				   expo, st->bases[idx], flo);

			/* record bases */
			st->bases[idx] = new_v[idx];
			st->expos[idx] = expo;
			is_non_nil = 1;
		} else if (st->expos[idx] != 0.0) {
			double tot_flo = new_v[idx] - st->bases[idx];

			flo = tot_flo * st->expos[idx];
			TRUF_DEBUG("NO %+.8g @ %.8g (- %.8g) -> %.8g => %.8g\n",
				   expo, new_v[idx], st->bases[idx],
				   flo, flo + st->bases[idx]);
			st->bases[idx] = new_v[idx];
			is_non_nil = 1;
		} else {
			/* st->expos[idx] == 0.0 && st->expos[idx] == expo */
			flo = 0.0;
			cut_rem_cc(c, c->comps + i);
		}
		/* munch it all together */
		res += flo;
	}
	st->was_non_nil = st->is_non_nil;
	st->is_non_nil = is_non_nil;
	st->inc_flo = res;
	st->cum_flo += res;
	return st->e;
}

static cutflo_trans_t
cut_base(struct __cutflo_st_s *st, trcut_t c, idate_t dt)
{
	double res = 0.0;
	const double *new_v = NULL;
	int is_non_nil = 0;

	for (size_t i = st->dvv_idx; i < st->tsc->ndvvs; i++) {
		if (st->tsc->dvvs[i].d == dt) {
			new_v = st->tsc->dvvs[i].v;
			st->dvv_idx = i + 1;
			break;
		}
	}
	for (size_t i = 0; i < c->ncomps; i++) {
		char mon = c->comps[i].month;
		uint16_t year = c->comps[i].year;
		trym_t ym = cym_to_ym(mon, year);
		double expo;
		ssize_t idx;
		double flo;

		if (ym == 0) {
			continue;
		}
		expo = c->comps[i].y * st->tick_val;

		if ((idx = tsc_find_cym_idx(st->tsc, ym)) < 0 ||
		    isnan(new_v[idx])) {
			if (expo != 0.0) {
				warn_noquo(dt, mon, year, expo);
			} else {
				cut_rem_cc(c, c->comps + i);
			}
			continue;
		}
		/* check for transition changes */
		if (expo != 0.0) {
			double tot_flo;

			if (UNLIKELY(isnan(st->basis))) {
				st->basis = 0.0;
			}

			tot_flo = (new_v[idx] - st->basis);

			flo = tot_flo * expo;
			TRUF_DEBUG("NO %+.8g @ %.8g => %.8g\n",
				   expo, tot_flo, flo);
			is_non_nil = 1;
		} else {
			flo = 0.0;
			cut_rem_cc(c, c->comps + i);
		}
		/* munch it all together */
		res += flo;
	}
	st->was_non_nil = st->is_non_nil;
	st->is_non_nil = is_non_nil;
	st->inc_flo = res;
	st->cum_flo += res;
	return st->e;
}

static cutflo_trans_t
cut_sparse(struct __cutflo_st_s *st, trcut_t c, idate_t dt)
{
	double res = 0.0;
	const double *new_v = NULL;
	int is_non_nil = 0;
	int has_trans = 0;

	for (size_t i = st->dvv_idx; i < st->tsc->ndvvs; i++) {
		if (st->tsc->dvvs[i].d == dt) {
			new_v = st->tsc->dvvs[i].v;
			st->dvv_idx = i + 1;
			break;
		}
	}
	for (size_t i = 0; i < c->ncomps; i++) {
		char mon = c->comps[i].month;
		uint16_t year = c->comps[i].year;
		trym_t ym = cym_to_ym(mon, year);
		double expo;
		ssize_t idx;
		double flo;

		if (ym == 0) {
			continue;
		}
		expo = c->comps[i].y * st->tick_val;

		if ((idx = tsc_find_cym_idx(st->tsc, ym)) < 0 ||
		    isnan(new_v[idx])) {
			if (expo != 0.0) {
				warn_noquo(dt, mon, year, expo);
			} else {
				cut_rem_cc(c, c->comps + i);
			}
			continue;
		}
		/* check for transition changes */
		if (st->expos[idx] != expo) {
			if (st->expos[idx] != 0.0) {
				double tot_flo = new_v[idx] - st->bases[idx];
				double trans_expo = st->expos[idx] - expo;
				flo = tot_flo * trans_expo;
			} else {
				flo = 0.0;
				/* guess a basis if the user asked us to */
				if (isnan(st->basis)) {
					st->basis = 0.0;
				}
			}

			TRUF_DEBUG("TR %+.8g @ %.8g -> %+.8g @ %.8g -> %.8g\n",
				   expo - st->expos[idx], new_v[idx],
				   expo, st->bases[idx], flo);

			/* record bases */
			if (st->expos[idx] == 0.0 || expo == 0.0) {
				st->bases[idx] = new_v[idx];
			}
			st->expos[idx] = expo;
			is_non_nil = 1;
			has_trans = 1;
		} else {
			/* st->expos[idx] == 0.0 && st->expos[idx] == expo */
			flo = 0.0;
			has_trans = 0;
			cut_rem_cc(c, c->comps + i);
		}
		/* munch it all together */
		res += flo;
	}
	st->has_trans = has_trans;
	st->was_non_nil = st->is_non_nil;
	st->is_non_nil = is_non_nil;
	st->inc_flo = res;
	st->cum_flo += res;
	return st->e;
}


struct __series_spec_s {
	const char *ser_file;
	double tick_val;
	double basis;
	unsigned int cump:1;
	unsigned int abs_dimen_p:1;
	unsigned int sparsep:1;
};

static cutflo_trans_t(*pick_cf_fun(struct __series_spec_s ser_sp))
	(struct __cutflo_st_s*, trcut_t, idate_t)
{
	if (LIKELY(!ser_sp.abs_dimen_p && !ser_sp.sparsep)) {
		return cut_flow;
	} else if (!ser_sp.sparsep) {
		return cut_base;
	} else {
		return cut_sparse;
	}
}

static void
roll_series(trsch_t s, struct __series_spec_s ser_sp, FILE *whither)
{
	trtsc_t ser;
	FILE *f;
	trcut_t c = NULL;
	struct __cutflo_st_s cfst;
	cutflo_trans_t(*const cf)(struct __cutflo_st_s*, trcut_t, idate_t) =
		pick_cf_fun(ser_sp);
	const unsigned int trbit = UNLIKELY(ser_sp.sparsep)
		? CUTFLO_HAS_TRANS_BIT : CUTFLO_TRANS_NON_NIL;

	if ((f = fopen(ser_sp.ser_file, "r")) == NULL) {
		fprintf(stderr, "could not open file %s\n", ser_sp.ser_file);
		return;
	} else if ((ser = read_series(f)) == NULL) {
		return;
	}

	/* init out cut flow state structure */
	init_cutflo_st(&cfst, ser, ser_sp.tick_val, ser_sp.basis);

	/* find the earliest date */
	for (size_t i = 0; i < ser->ndvvs; i++) {
		idate_t dt = ser->dvvs[i].d;
		daysi_t mc_ds = idate_to_daysi(dt);

		/* anchor now contains the very first date and value */
		if ((c = make_cut(c, s, mc_ds)) == NULL) {
			continue;
		}

		if (cf(&cfst, c, dt) > trbit) {
			char buf[32];
			double val;

			if (LIKELY(!ser_sp.abs_dimen_p && ser_sp.cump)) {
				val = cfst.cum_flo + cfst.basis;
			} else if (LIKELY(!ser_sp.abs_dimen_p)) {
				val = cfst.inc_flo;
			} else if (LIKELY(!ser_sp.cump)) {
				val = cfst.inc_flo;
			} else {
				val = cfst.cum_flo;
			}
			snprint_idate(buf, sizeof(buf), dt);
			fprintf(whither, "%s\t%.8g\n", buf, val);
		}
	}

	/* free resources */
	if (c) {
		free_cut(c);
	}
	/* free up resources */
	free_cutflo_st(&cfst);
	if (ser) {
		free_series(ser);
	}
	fclose(f);
	return;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "truffle-clo.h"
#include "truffle-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	trsch_t sch = NULL;
	int res = 0;

	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	if (argi->schema_given) {
		sch = read_schema(argi->schema_arg);
	} else {
		sch = read_schema("-");
	}
	if (UNLIKELY(sch == NULL)) {
		fputs("schema unreadable\n", stderr);
		res = 1;
		goto sch_out;
	}
	/* finally call our main routine */
	if (argi->series_given) {
		struct __series_spec_s sp = {
			.ser_file = argi->series_arg,
			.tick_val = argi->tick_value_given
			? argi->tick_value_arg : 1.0,
			.basis = argi->basis_given
			? argi->basis_arg : NAN,
			.cump = !argi->flow_given,
			.abs_dimen_p = argi->abs_dimen_given,
			.sparsep = argi->sparse_given,
		};
		roll_series(sch, sp, stdout);
	} else if (argi->inputs_num == 0) {
		print_schema(sch, stdout);
	} else {
		struct trcut_pr_s opt = {
			.abs = argi->abs_given,
			.oco = argi->oco_given,
			.rnd = argi->round_given,
			.lever = argi->lever_given ? argi->lever_arg : 1.0,
			.out = stdout,
		};
		trcut_t c = NULL;

		for (size_t i = 0; i < argi->inputs_num; i++) {
			idate_t dt = read_date(argi->inputs[i], NULL);
			daysi_t ds = idate_to_daysi(dt);

			if ((c = make_cut(c, sch, ds))) {
				print_cut(c, dt, opt);
			}
		}
		if (c) {
			free_cut(c);
		}
	}
	free_schema(sch);
	/* just to make sure */
	fflush(stdout);
sch_out:
	cmdline_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* truffle.c ends here */
