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

#include <stdint.h>
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


DEFUN trym_t
read_trym(const char *str, const char **restrict ptr)
{
	const char *sp = str;
	const char *sq;
	unsigned int ym;
	unsigned int mo;
	unsigned int yr;
	trym_t res;

	if ((mo = m_to_i(*sp))) {
		sp++;
	}
	/* snarf off the bit after the month */
	ym = strtoui(sp, &sq);
	/* go for the detailed inspection */
	if (UNLIKELY(sq == sp)) {
		/* completely fucked innit */
		return 0U;
	} else if (ym < TRYM_YR_CUTOFF) {
		/* something like G0 or F4 or so */
		yr = ym;
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
	} else if (ym < 299913U) {
		/* that's %Y%m syntax innit? */
		yr = ym / 100U;
		mo = ym % 100U;
	} else if (ym < 29991232U) {
		/* %Y%m%d syntax but we can't deal with d */
		yr = (ym / 100U) / 100U;
		mo = (ym / 100U) % 100U;
	}
	res = cym_to_trym(yr, mo);
	/* assign end pointer */
	if (ptr) {
		*ptr = (const char*)sq;
	}
	return res;
}

/* mmy.c ends here */
