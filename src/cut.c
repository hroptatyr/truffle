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
print_cut(trcut_t c, idate_t dt, struct trcut_pr_s opt)
{
	char buf[64U];
	int y = dt / 10000;
	char *p = buf;
	char *var;

	if (opt.abs) {
		c->year_off = (uint16_t)(dt / 10000U);
	}
	p += snprint_idate(buf, sizeof(buf), dt);
	*p++ = '\t';
	var = p;
	for (size_t i = 0; i < c->ncomps; i++, p = var) {
		if (c->comps[i].month == 0) {
			continue;
		}

		switch (c->type) {
		case TRCUT_LEVER:
			break;

		case TRCUT_EDGE:
			if (c->ecomps[i].val == 2U) {
				continue;
			}
			if (!c->ecomps[i].val) {
				*p++ = '~';
			}
			break;

		default:
			break;
		}

		if (!opt.oco) {
			p += snprintf(
				p, sizeof(buf) - (p - buf),
				"%c%d",
				c->comps[i].month,
				c->comps[i].year - y + c->year_off);
		} else {
			p += snprintf(
				p, sizeof(buf) - (p - buf),
				"%d%02d",
				c->comps[i].year - y + c->year_off,
				m_to_i(c->comps[i].month));
		}

		switch (c->type) {
			double expo;

		case TRCUT_LEVER: {
			expo = c->comps[i].y * opt.lever;

			if (opt.rnd) {
				expo = round(expo);
			}

			*p++ = '\t';
			p += snprintf(
				p, sizeof(buf) - (p - buf), "%.8g", expo);
			break;
		}

		case TRCUT_EDGE:
			break;

		default:
			break;
		}

		cut_rem_cc(c, c->comps + i);

		*p++ = '\n';
		*p = '\0';
		fputs(buf, opt.out);
	}
	return;
}

/* cut.c ends here */
