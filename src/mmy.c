/*** mmy.c -- convert between mmy and numeric month year contracts
 *
 * Copyright (C) 2013 Sebastian Freundt
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
#include <stdio.h>
#include <string.h>
#include "mmy.h"
#include "nifty.h"


static inline char
i_to_m(unsigned int month)
{
	static char months[] = "?FGHJKMNQUVXZ";
	return months[month];
}

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

static uint32_t
strtoui(const char *str, const char *ep[static 1])
{
	uint32_t res = 0U;
	const char *sp;

	for (sp = str; *sp >= '0' && *sp <= '9'; sp++) {
		res *= 10;
		res += *sp - '0';
	}
	*ep = (const char*)sp;
	return res;
}


/* public API */
truf_mmy_t
truf_mmy_rd(const char *str, char **ptr)
{
	const char *sp = str;
	const char *sq;
	unsigned int ym;
	unsigned int dd;
	unsigned int mo;
	unsigned int yr;
	truf_mmy_t res;

	if ((mo = m_to_i(*sp))) {
		sp++;
	}
	/* snarf off the bit after the month */
	ym = strtoui(sp, &sq);
	/* go for the detailed inspection */
	if (UNLIKELY(sq == sp)) {
		/* completely fucked innit */
		sq = strchr(sp, '\t');
		res = 0U;
		goto yld;
	} else if (ym < TRUF_MMY_ABSYR) {
		/* something like G0 or F4 or so */
		yr = ym;
		dd = 0U;
	} else if (ym < 4096U) {
		/* absolute year, G2032 or H2102*/
		if (!mo && *sq == '-') {
			/* %Y-%m syntax? */
			sp = sq + 1;
			mo = strtoui(sp, &sq);
			if (UNLIKELY(sq <= sp || mo > 12U)) {
				/* reset mo */
				mo = 0U;
			}
		}
		yr = ym;
		/* no days mentioned in this one */
		dd = 0U;
	} else if (ym < 299913U) {
		/* that's %Y%m syntax innit? */
		yr = ym / 100U;
		mo = ym % 100U;
		dd = -1U;
	} else if (ym < 29991232U) {
		/* %Y%m%d syntax */
		yr = (ym / 100U) / 100U;
		mo = (ym / 100U) % 100U;
		dd = (ym % 100U);
	}
	res = make_truf_mmy(yr, mo, dd);
	/* assign end pointer */
yld:
	if (ptr) {
		*ptr = deconst(sq);
	}
	return res;
}

size_t
truf_mmy_wr(char *restrict buf, size_t bsz, truf_mmy_t ym)
{
	register unsigned int d = truf_mmy_day(ym);
	register unsigned int m = truf_mmy_mon(ym);
	register signed int y = truf_mmy_year(ym);

	if (truf_mmy_abs_p(ym) && d) {
		if (d < 32U) {
			return snprintf(buf, bsz, "%d%02u%02u", y, m, d);
		}
		/* oco? */
		return snprintf(buf, bsz, "%d%02u", y, m);
	}
	/* kick d as it doesn't work with relative mmys */
	return snprintf(buf, bsz, "%c%d", i_to_m(m), y);
}

truf_sym_t
truf_sym_rd(const char *str, char **on)
{
	truf_sym_t res;
	char *tmp;

	if ((res = truf_mmy_rd(str, &tmp)) == 0U && LIKELY(tmp != NULL)) {
		static char *strbuf;
		static size_t strbuz;
		size_t len;

		/* take string over to strbuf */
		if ((len = tmp - str) > strbuz) {
			strbuz = (len / 64U + 1U) * 64U;
			strbuf = realloc(strbuf, strbuz);
		}
		memcpy(strbuf, str, len);
		strbuf[len] = '\0';
		res = (truf_sym_t)strbuf;
	}
	if (LIKELY(on != NULL)) {
		*on = tmp;
	}
	return res;
}

truf_sym_t
truf_sym_rd_alloc(const char *str, char **on)
{
	truf_sym_t res;
	char *tmp;

	if ((res = truf_mmy_rd(str, &tmp)) == 0U && LIKELY(tmp != NULL)) {
		size_t len = tmp - str;
		char *str_clon;

		/* take string over to strbuf */
		str_clon = strndup(str, len);
		res = (truf_sym_t)str_clon;
	}
	if (LIKELY(on != NULL)) {
		*on = tmp;
	}
	return res;
}

/* mmy.c ends here */
