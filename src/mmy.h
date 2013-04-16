/*** mmy.h -- convert between mmy and numeric month year contracts
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
#if !defined INCLUDED_mmy_h_
#define INCLUDED_mmy_h_

#include <stdint.h>

#if !defined DECLF
# define DECLF		extern
# define DEFUN
#endif	/* !DECLF */

/* our notion of MMY */
typedef int32_t trym_t;
#define TRYM_WIDTH	(24U)

/* first year interpreted as absolute */
#define TRYM_YR_CUTOFF	(1024)


DECLF trym_t read_trym(const char *str, const char **restrict ptr);


static inline __attribute__((pure, const)) trym_t
cym_to_trym(unsigned int year, unsigned int mon)
{
	return (year << 8U) | (mon & 0xffU);
}

static inline __attribute__((pure, const)) int
trym_yr(trym_t ym)
{
	return ym >> 8U;
}

static inline __attribute__((pure, const)) unsigned int
trym_mo(trym_t ym)
{
	return ym & 0xffU;
}

static inline __attribute__((pure, const)) trym_t
rel_trym(trym_t ym, int year)
{
/* return a trym relative to YEAR, i.e. F0 for F2000 for year == 2000 */
	return cym_to_trym(trym_yr(ym) - year, ym);
}

static inline __attribute__((pure, const)) trym_t
abs_trym(trym_t ym, int year)
{
	return cym_to_trym(trym_yr(ym) + year, ym);
}

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

#endif	/* INCLUDED_mmy_h_ */
