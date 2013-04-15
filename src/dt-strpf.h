/*** dt-strpf.h -- date parsing and formatting
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
#if !defined INCLUDED_dt_strpf_h_
#define INCLUDED_dt_strpf_h_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "yd.h"

#if !defined DECLF
# define DECLF		extern
# define DEFUN
#endif	/* !DECLF */

/* date/time goodies */
#define BASE_YEAR	(1917U)
#define TO_BASE(x)	((x) - BASE_YEAR)
#define TO_YEAR(x)	((x) + BASE_YEAR)

typedef uint32_t idate_t;
typedef uint32_t daysi_t;
typedef union trod_instant_u trod_instant_t;

union trod_instant_u {
	struct {
		uint32_t y:16;
		uint32_t m:8;
		uint32_t d:8;
		uint32_t H:8;
		uint32_t M:8;
		uint32_t S:6;
		uint32_t ms:10;
	};
	uint64_t u;
} __attribute__((transparent_union));


/* public api */
DECLF idate_t read_date(const char *str, char **restrict ptr);

DECLF trod_instant_t dt_strp(const char *str);

DECLF size_t dt_strf(char *restrict buf, size_t bsz, trod_instant_t inst);

/**
 * Fix up instants like the 32 Dec to become 01 Jan of the following year. */
DECLF trod_instant_t trod_instant_fixup(trod_instant_t);

DECLF trod_instant_t daysi_to_trod_instant(daysi_t);


static inline daysi_t
idate_to_daysi(idate_t dt)
{
/* compute days since BASE-01-00 (Mon),
 * if year slot is absent in D compute the day in the year of D instead. */
	int d = dt % 100U;
	int m = (dt / 100U) % 100U;
	int y = (dt / 100U) / 100U;
	struct yd_s yd = __md_to_yd(y, (struct md_s){.m = m, .d = d});
	int by = TO_BASE(y);

	return by * 365U + by / 4U + yd.d;
}

static inline size_t
snprint_idate(char *restrict buf, size_t bsz, idate_t dt)
{
	return snprintf(buf, bsz, "%u-%02u-%02u",
			dt / 10000, (dt % 10000) / 100, (dt % 100));
}


/* instant helpers */
static inline __attribute__((pure)) bool
trod_inst_0_p(trod_instant_t x)
{
	return x.u == 0U;
}

static inline __attribute__((pure)) bool
trod_inst_lt_p(trod_instant_t x, trod_instant_t y)
{
	return (x.y < y.y || x.y == y.y &&
		(x.m < y.m || x.m == y.m &&
		 (x.d < y.d || x.d == y.d &&
		  (x.H < y.H || x.H == y.H &&
		   (x.M < y.M || x.M == y.M &&
		    (x.S < y.S || x.S == y.S &&
		     (x.ms < y.ms)))))));
}

static inline __attribute__((pure)) bool
trod_inst_le_p(trod_instant_t x, trod_instant_t y)
{
	return !(x.y > y.y || x.y == y.y &&
		 (x.m > y.m || x.m == y.m &&
		  (x.d > y.d || x.d == y.d &&
		   (x.H > y.H || x.H == y.H &&
		    (x.M > y.M || x.M == y.M &&
		     (x.S > y.S || x.S == y.S &&
		      (x.ms > y.ms)))))));
}

static inline __attribute__((pure)) bool
trod_inst_eq_p(trod_instant_t x, trod_instant_t y)
{
	return x.y == y.y && x.m == y.m && x.d == y.d &&
		x.H == y.H && x.M == y.M && x.S == y.S &&
		x.ms == y.ms;
}


#define TROD_ALL_DAY	(0xffU)
#define TROD_ALL_SEC	(0x3ffU)

static inline bool
trod_instant_all_day_p(trod_instant_t i)
{
	return i.H == TROD_ALL_DAY;
}

static inline bool
trod_instant_all_sec_p(trod_instant_t i)
{
	return i.ms == TROD_ALL_SEC;
}

#endif	/* INCLUDED_dt_strpf_h_ */
