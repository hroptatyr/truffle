/*** dt-strpf.c -- date parsing and formatting
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

static char*
__p2c(const char *str)
{
	union {
		const char *c;
		char *p;
	} this = {.c = str};
	return this.p;
}


/* public api */
DEFUN idate_t
read_date(const char *str, char **restrict ptr)
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
	if (ptr) {
		*ptr = __p2c(tmp);
	}
#undef C
	return res;
}

static int32_t
strtoi_lim(const char *str, const char **ep, int32_t llim, int32_t ulim)
{
	int32_t res = 0;
	const char *sp;
	/* we keep track of the number of digits via rulim */
	int32_t rulim;

	for (sp = str, rulim = ulim > 10 ? ulim : 10;
	     res * 10 <= ulim && rulim && *sp >= '0' && *sp <= '9';
	     sp++, rulim /= 10) {
		res *= 10;
		res += *sp - '0';
	}
	if (UNLIKELY(sp == str)) {
		res = -1;
	} else if (UNLIKELY(res < llim || res > ulim)) {
		res = -2;
	}
	*ep = (const char*)sp;
	return res;
}

static size_t
ui32tostr(char *restrict buf, size_t bsz, uint32_t d, int pad)
{
/* all strings should be little */
#define C(x, d)	(char)((x) / (d) % 10 + '0')
	size_t res;

	if (UNLIKELY(d > 1000000000)) {
		return 0;
	}
	switch ((res = (size_t)pad) < bsz ? res : bsz) {
	case 9:
		/* for nanoseconds */
		buf[pad - 9] = C(d, 100000000);
		buf[pad - 8] = C(d, 10000000);
		buf[pad - 7] = C(d, 1000000);
	case 6:
		/* for microseconds */
		buf[pad - 6] = C(d, 100000);
		buf[pad - 5] = C(d, 10000);
	case 4:
		/* for western year numbers */
		buf[pad - 4] = C(d, 1000);
	case 3:
		/* for milliseconds or doy et al. numbers */
		buf[pad - 3] = C(d, 100);
	case 2:
		/* hours, mins, secs, doms, moys, etc. */
		buf[pad - 2] = C(d, 10);
	case 1:
		buf[pad - 1] = C(d, 1);
		break;
	default:
	case 0:
		res = 0;
		break;
	}
	return res;
}


DEFUN trod_instant_t
dt_strp(const char *str)
{
/* code dupe, see __strpd_std() */
	trod_instant_t nul = {0};
	trod_instant_t res = {0};
	const char *sp;
	int32_t tmp;

	if (UNLIKELY((sp = str) == NULL)) {
		return nul;
	}
	/* read the year */
	if ((tmp = strtoi_lim(sp, &sp, 1583, 4095)) < 0 || *sp++ != '-') {
		return nul;
	}
	res.y = tmp;

	/* read the month */
	if ((tmp = strtoi_lim(sp, &sp, 0, 12)) < 0 || *sp++ != '-') {
		return nul;
	}
	res.m = tmp;

	/* read the day or the count */
	if ((tmp = strtoi_lim(sp, &sp, 0, 31)) < 0) {
		return nul;
	}
	res.d = tmp;

	/* check for the d/t separator */
	switch (*sp++) {
	case ' ':
		/* time might be following */
	case 'T':
		/* time's following */
		break;
	case '\0':
	default:
		/* just the date, make it TROD_ALL_DAY then aye */
		res.H = TROD_ALL_DAY;
		return res;
	}

	/* and now parse the time */
	if ((tmp = strtoi_lim(sp, &sp, 0, 23)) < 0 || *sp++ != ':') {
		return nul;
	}
	res.H = tmp;

	/* minute */
	if ((tmp = strtoi_lim(++sp, &sp, 0, 59)) < 0 || *sp++ != ':') {
		return nul;
	}
	res.M = tmp;

	/* second, allow leap second too */
	if ((tmp = strtoi_lim(++sp, &sp, 0, 60)) < 0) {
		return nul;
	}
	res.S = tmp;

	/* millisecond part */
	if (*sp++ != '.') {
		/* make it ALL_SEC then */
		tmp = TROD_ALL_SEC;
	} else if ((tmp = strtoi_lim(sp, &sp, 0, 999)) < 0) {
		return nul;
	}
	res.ms = tmp;
	return res;
}

DEFUN size_t
dt_strf(char *restrict buf, size_t bsz, trod_instant_t inst)
{
	char *restrict bp = buf;
#define bz	(bsz - (bp - buf))

	bp += ui32tostr(bp, bz, inst.y, 4);
	*bp++ = '-';
	bp += ui32tostr(bp, bz, inst.m, 2);
	*bp++ = '-';
	bp += ui32tostr(bp, bz, inst.d, 2);

	if (LIKELY(!trod_instant_all_day_p(inst))) {
		*bp++ = 'T';
		bp += ui32tostr(bp, bz, inst.H, 2);
		*bp++ = ':';
		bp += ui32tostr(bp, bz, inst.M, 2);
		*bp++ = ':';
		bp += ui32tostr(bp, bz, inst.S, 2);
		if (LIKELY(!trod_instant_all_sec_p(inst))) {
			*bp++ = '.';
			bp += ui32tostr(bp, bz, inst.ms, 3);
		}
	}
	*bp = '\0';
	return bp - buf;
}

DEFUN trod_instant_t
trod_instant_fixup(trod_instant_t e)
{
/* this is basically __ymd_fixup_d of dateutils
 * we only care about additive cockups though because instants are
 * chronologically ascending */
	static const unsigned int mdays[] = {
		0U, 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U,
	};
	unsigned int md;

	if (UNLIKELY(trod_instant_all_day_p(e))) {
		/* just fix up the day, dom and year portion */
		goto fixup_d;
	} else if (UNLIKELY(trod_instant_all_day_p(e))) {
		/* just fix up the sec, min, ... portions */
		goto fixup_S;
	}

	if (UNLIKELY(e.ms >= 1000U)) {
		unsigned int dS = e.ms / 1000U;
		unsigned int ms = e.ms % 1000U;

		e.ms = ms;
		e.S += dS;
	}

fixup_S:
	if (UNLIKELY(e.S >= 60U)) {
		/* leap seconds? */
		unsigned int dM = e.S / 60U;
		unsigned int S = e.S % 60U;

		e.S = S;
		e.M += dM;
	}
	if (UNLIKELY(e.M >= 60U)) {
		unsigned int dH = e.M / 60U;
		unsigned int M = e.M % 60U;

		e.M = M;
		e.H += dH;
	}
	if (UNLIKELY(e.H >= 24U)) {
		unsigned int dd = e.H / 24U;
		unsigned int H = e.H % 24U;

		e.H = H;
		e.d += dd;
	}

fixup_d:
refix_ym:
	if (UNLIKELY(e.m > 12U)) {
		unsigned int dy = (e.m - 1) / 12U;
		unsigned int m = (e.m - 1) % 12U + 1U;

		e.m = m;
		e.y += dy;
	}

	if (UNLIKELY(e.d > (md = mdays[e.m]))) {
		/* leap year handling */
		if (UNLIKELY(e.m == 2U && (e.y % 4U) == 0U)) {
			md++;
		}
		if (LIKELY((e.d -= md) > 0U)) {
			e.m++;
			goto refix_ym;
		}
	}
	return e;
}

DEFUN trod_instant_t
daysi_to_trod_instant(daysi_t dd)
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
	return (trod_instant_t){y, m, d, TROD_ALL_DAY};
}

/* dt-strpf.c ends here */
