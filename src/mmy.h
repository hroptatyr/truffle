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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * truf_mmys specify a contract's maturity date or the classic MMY.
 *
 * They fit neatly into a uintptr, taking only 32bits of space but
 * can be cast to const char* for arbitrary symbols.
 *
 * In case of mmy notation the layout is:
 * YYYYYYYYYYYYYYYY MMMMMMMM DDDDDDD1
 * ^^^^^^^^^^^^^^^^ ^^^^^^^^ ^^^^^^^|
 * year (rel/abs)   month    day    to distinguish from const char* 
 *
 * Years below TRUF_SYM_ABSYR are relative.
 * If the maturity lacks a date (classic mmy) the day slot shall
 * be zeroed out.
 *
 * In either case */
typedef intptr_t truf_mmy_t;

/* first year interpreted as absolute */
#define TRUF_MMY_ABSYR	(1024)


/**
 * Try and read the string STR as MMY notation and return a truf sym object. */
extern truf_mmy_t truf_mmy_rd(const char *str, const char **restrict ptr);


static inline __attribute__((pure, const)) truf_mmy_t
make_truf_mmy(signed int year, unsigned int mon, unsigned int day)
{
	mon &= 0xffU;
	day &= 0x7fU;
	return (((((year << 8U) | mon) << 7U) | day) << 1U) | 1U;
}

static inline __attribute__((pure, const)) signed int
truf_mmy_year(truf_mmy_t ym)
{
	return ym >> 16U;
}

static inline __attribute__((pure, const)) unsigned int
truf_mmy_mon(truf_mmy_t ym)
{
	return (ym >> 8U) & 0xffU;
}

static inline __attribute__((pure, const)) unsigned int
truf_mmy_day(truf_mmy_t ym)
{
	return (ym >> 1U) & 0x7fU;
}

static inline __attribute__((pure, const)) bool
truf_mmy_p(truf_mmy_t ym)
{
/* return true if YM encodes a MMY and false if it is a pointer */
	return (ym & 0b1U);
}

static inline __attribute__((pure, const)) bool
truf_mmy_abs_p(truf_mmy_t ym)
{
/* return true if YM is in absolute notation. */
	register signed int yr = truf_mmy_year(ym);
	return yr >= TRUF_MMY_ABSYR;
}

static inline __attribute__((pure, const)) truf_mmy_t
truf_mmy_rel(truf_mmy_t ym, unsigned int year)
{
/* return a trym relative to YEAR, i.e. F0 for F2000 for year == 2000
 * or leave as is if YM is relative already. */
	register signed int yr = truf_mmy_year(ym);
	if (truf_mmy_abs_p(ym)) {
		register signed int m = truf_mmy_mon(ym);
		register signed int d = truf_mmy_day(ym);
		return make_truf_mmy(yr - year, m, d);
	}
	return ym;
}

static inline __attribute__((pure, const)) truf_mmy_t
truf_mmy_abs(truf_mmy_t ym, unsigned int year)
{
/* return the absolute version of YM relative to YEAR,
 * i.e. F0 goes to F2000 for year == 2000
 * or leave as is if YM is absolute already. */
	register signed int yr = truf_mmy_year(ym);
	if (!truf_mmy_abs_p(ym)) {
		register signed int m = truf_mmy_mon(ym);
		register signed int d = truf_mmy_day(ym);
		return make_truf_mmy(yr + year, m, d);
	}
	return ym;
}

#endif	/* INCLUDED_mmy_h_ */
