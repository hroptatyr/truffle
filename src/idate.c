/*** idate.c -- integer coded dates
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
#include <ctype.h>
#include <stdio.h>
#include "idate.h"
#include "nifty.h"
#include "yd.h"


/* public api */
idate_t
read_idate(const char *str, char **restrict on)
{
#define C(x)	(x - '0')
	idate_t res = 0;
	const char *tmp;

	tmp = str;
	if (!isdigit(*tmp)) {
		goto out;
	}

	/* start off populating res */
	res = C(tmp[0]) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (!isdigit(*tmp)) {
		goto out;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (!isdigit(*tmp)) {
		/* already buggered? */
		goto out;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (!isdigit(*tmp)) {
		/* date is fucked? */
		goto out;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2;

out:
	if (LIKELY(on != NULL)) {
		*on = deconst(tmp);
	}
#undef C
	return res;
}

size_t
prnt_idate(char *restrict buf, size_t bsz, idate_t dt)
{
	unsigned int y = idate_y(dt);
	unsigned int m = idate_m(dt);
	unsigned int d = idate_d(dt);

	return snprintf(buf, bsz, "%u-%02u-%02u", y, m, d);
}

__attribute__((pure, const)) daisy_t
daisy_sans_year(idate_t id)
{
	int d = (id % 100U);
	int m = (id / 100U) % 100U;
	struct yd_s yd = __md_to_yd(2000, (struct md_s){.m = m, .d = d});
	daisy_t doy = yd.d | DAISY_DIY_BIT;
	return doy;
}

/* standalone version, we could use ds_sum but this is most likely
 * causing more cache misses */
__attribute__((pure, const)) idate_t
daisy_to_idate(daisy_t dd)
{
/* given days since 2000-01-00 (Mon),
 * compute the idate_t representation X so that idate_to_daysi(X) == DDT */
/* stolen from dateutils' daisy.c */
	int y;
	int m;
	int d;
	int j00;
	unsigned int doy;

	/* get year first (estimate) */
	y = dd / 365U;
	/* get jan-00 of (est.) Y */
	j00 = y * 365U + y / 4U;
	/* y correct? */
	if (UNLIKELY(j00 >= (int)dd)) {
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
	return (y * 100U + m) * 100U + d;
}

__attribute__((pure, const)) daisy_t
idate_to_daisy(idate_t dt)
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

/* idate.c ends here */
