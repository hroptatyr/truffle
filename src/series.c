/*** series.c -- read time series of quotes or settlement prices
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
#include <string.h>
#include <sys/mman.h>
#include "series.h"
#include "dt-strpf.h"
#include "mmy.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */

struct __dv_s {
	idate_t d;
	double v;
};

#define TSC_STEP	(4096)
#define CYM_STEP	(256)


/* mmap helpers */
#if !defined MAP_ANON && defined MAP_ANONYMOUS
# define MAP_ANON	MAP_ANONYMOUS
#elif defined MAP_ANON
/* all's good */
#else  /* !MAP_ANON && !MAP_ANONYMOUS */
# define MAP_ANON	(0U)
#endif	/* !MAP_ANON && MAP_ANONYMOUS */

#if !defined MAP_MEM
# define MAP_MEM	(MAP_ANON | MAP_PRIVATE)
#endif	/* !MAP_MEM */
#if !defined PROT_MEM
# define PROT_MEM	(PROT_READ | PROT_WRITE)
#endif	/* !PROT_MEM */

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


/* libc malloc helpers */
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


/* helpers */
static struct __dvv_s*
tsc_init_dvv(trtsc_t s, size_t idx, idate_t dt)
{
	struct __dvv_s *t = s->dvvs + idx;
	t->d = dt;
	/* make room for s->ncons doubles and set them to nan */
	upsize_mall((void**)&t->v, 0, s->ncons, sizeof(*t->v), CYM_STEP, -1);
	return t;
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
	if ((idx = tsc_find_cym_idx(s, cym_to_trym(yoff, mon))) < 0) {
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
		s->cons[idx] = cym_to_trym(yoff, mon);
	}
	/* resize the double vector maybe */
	this->v[idx] = dv.v;
	return;
}


/* public api */
DEFUN trtsc_t
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

DEFUN trtsc_t
read_series_from_file(const char *file)
{
	trtsc_t ser;
	FILE *f;

	if ((f = fopen(file, "r")) == NULL) {
		return NULL;
	} else if ((ser = read_series(f)) == NULL) {
		return NULL;
	}
	/* close this one now */
	fclose(f);
	return ser;
}

DEFUN void
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

/* series.c ends here */
