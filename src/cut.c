/*** cut.c -- cuts along the date axis
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
#include <string.h>
#include <math.h>
#include "truffle.h"
#include "cut.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */


static int
m_to_i(char month)
{
	switch (month) {
	case 'F':
		return 1;
	case 'G':
		return 2;
	case 'H':
		return 3;
	case 'J':
		return 4;
	case 'K':
		return 5;
	case 'M':
		return 6;
	case 'N':
		return 7;
	case 'Q':
		return 8;
	case 'U':
		return 9;
	case 'V':
		return 10;
	case 'X':
		return 11;
	case 'Z':
		return 12;
	default:
		return 0;
	}
}

static struct trcc_s*
__cut_find_cc(trcut_t c, uint8_t mon, uint16_t year)
{
	for (size_t i = 0; i < c->ncomps; i++) {
		if (c->comps[i].month == mon && c->comps[i].year == year) {
			return c->comps + i;
		}
	}
	return NULL;
}


/* public API */
DEFUN trcut_t
cut_add_cc(trcut_t c, struct trcc_s cc)
{
	struct trcc_s *prev;

	if (c == NULL) {
		size_t new = sizeof(*c) + 16 * sizeof(*c->comps);
		c = calloc(new, 1);
	} else if ((prev = __cut_find_cc(c, cc.month, cc.year))) {
		prev->y = cc.y;
		return c;
	} else if ((prev = __cut_find_cc(c, 0, 0))) {
		*prev = cc;
		return c;
	} else if ((c->ncomps % 16) == 0) {
		size_t new = sizeof(*c) + (c->ncomps + 16) * sizeof(*c->comps);
		c = realloc(c, new);
		memset(c->comps + c->ncomps, 0, 16 * sizeof(*c->comps));
	}
	c->comps[c->ncomps++] = cc;
	return c;
}

DEFUN void
cut_rem_cc(trcut_t UNUSED(c), struct trcc_s *cc)
{
	cc->month = 0;
	cc->year = 0;
	return;
}

DEFUN void
print_cut(trcut_t c, idate_t dt, double lever, bool rnd, bool oco, FILE *out)
{
	char buf[32];
	int y = dt / 10000;

	snprint_idate(buf, sizeof(buf), dt);
	for (size_t i = 0; i < c->ncomps; i++) {
		double expo;

		if (c->comps[i].month == 0) {
			continue;
		}

		expo = c->comps[i].y * lever;

		if (rnd) {
			expo = round(expo);
		}
		if (!oco) {
			fprintf(out, "%s\t%c%d\t%.8g\n",
				buf,
				c->comps[i].month,
				c->comps[i].year - y + c->year_off,
				expo);
		} else {
			fprintf(out, "%s\t%d%02d\t%.8g\n",
				buf,
				c->comps[i].year - y + c->year_off,
				m_to_i(c->comps[i].month),
				expo);
		}
		cut_rem_cc(c, c->comps + i);
	}
	return;
}

/* cut.c ends here */
