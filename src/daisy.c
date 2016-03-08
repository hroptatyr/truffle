/*** daisy.c -- common daisy related goodies
 *
 * Copyright (C) 2011-2016 Sebastian Freundt
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
#include "instant.h"
#include "daisy.h"
#include "yd.h"

#if defined __INTEL_COMPILER

#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wmissing-braces"
# pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif	/* __INTEL_COMPILER || __GNUC__ */


/* public API */
__attribute__((pure, const)) daisy_t
instant_to_daisy(echs_instant_t i)
{
/* compute days since BASE-01-00 (Mon),
 * if year slot is absent in D compute the day in the year of D instead. */
	struct yd_s yd = __md_to_yd(i.y, (struct md_s){.m = i.m, .d = i.d});
	int by = TO_BASE(i.y);

	return by * 365U + by / 4U + yd.d;
}

__attribute__((pure, const)) echs_instant_t
daisy_to_instant(daisy_t dd)
{
/* given days since BASE-01-00,
 * compute the instant_t representation X */
/* stolen from dateutils' daisy.c */
	unsigned int y;
	unsigned int m;
	unsigned int d;
	unsigned int j00;
	unsigned int doy;

	/* get year first (estimate) */
	y = dd / 365U;
	/* get jan-00 of (est.) Y */
	j00 = y * 365U + y / 4U;
	/* y correct? */
	if (UNLIKELY(j00 >= dd)) {
		/* correct y */
		y--;
		/* and also recompute the j00 of y */
		j00 = y * 365U + y / 4U;
	}
	/* ass */
	y = TO_YEAR(y);
	/* this one must be positive now */
	doy = dd - j00;

	/* get month and day from doy */
	{
		struct md_s md = __yd_to_md((struct yd_s){y, doy});
		m = md.m;
		d = md.d;
	}
	return (echs_instant_t){y, m, d, ECHS_ALL_DAY};
}

/* standalone version and adapted to what make_cut() needs */
unsigned int
daisy_to_year(daisy_t dd)
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

daisy_t
daisy_in_year(daisy_t ds, unsigned int y)
{
	int j00;
	int by = TO_BASE(y);

	if (UNLIKELY(!(ds & DAISY_DIY_BIT))) {
		/* we could technically do something here */
		return ds;
	}

	ds &= ~DAISY_DIY_BIT;

	/* get jan-00 of (est.) Y */
	j00 = by * 365U + by / 4U;

	if (y % 4U != 0 && ds >= 60) {
		ds--;
	}
	return ds + j00;
}

/* daisy.c ends here */
