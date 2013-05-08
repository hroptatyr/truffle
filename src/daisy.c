/*** daisy.c -- common daisy related goodies
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
#include "truffle.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */


/* date/time goodies */
#define DAYSI_DIY_BIT	(1 << (sizeof(daysi_t) * 8 - 1))

static __attribute__((unused)) daysi_t
daysi_sans_year(idate_t id)
{
	int d = (id % 100U);
	int m = (id / 100U) % 100U;
	struct yd_s yd = __md_to_yd(2000, (struct md_s){.m = m, .d = d});
	daysi_t doy = yd.d | DAYSI_DIY_BIT;

	return doy;
}

static daysi_t
daysi_in_year(daysi_t ds, int y)
{
	int j00;
	int by = TO_BASE(y);

	if (UNLIKELY(!(ds & DAYSI_DIY_BIT))) {
		/* we could technically do something here */
		return ds;
	}

	ds &= ~DAYSI_DIY_BIT;

	/* get jan-00 of (est.) Y */
	j00 = by * 365U + by / 4U;

	if (y % 4U != 0 && ds >= 60) {
		ds--;
	}
	return ds + j00;
}

/* standalone version and adapted to what make_cut() needs */
static int
daysi_to_year(daysi_t dd)
{
	int y;
	int j00;

	/* get year first (estimate) */
	y = dd / 365U;
	/* get jan-00 of (est.) Y */
	j00 = y * 365U + y / 4U;
	/* y correct? */
	if (UNLIKELY(j00 >= (int)dd)) {
		/* correct y */
		y--;
	}
	/* ass */
	return TO_YEAR(y);
}

/* daisy.c ends here */
