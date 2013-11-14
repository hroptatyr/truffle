/*** truf-dfp754.c -- _Decimal32 goodness
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
#if defined HAVE_DFP754_H
# include <dfp754.h>
#elif defined HAVE_DFP_STDLIB_H
# include <dfp/stdlib.h>
#endif	/* HAVE_DFP754_H */
#include "truf-dfp754.h"
#include "nifty.h"

#define U(x)	(uint_least8_t)((x) - '0')
#define C(x)	(char)((x) + '0')

typedef struct bcd32_s bcd32_t;

struct bcd32_s {
	uint_least32_t mant;
	int expo;
	int sign;
};


static inline __attribute__((pure, const)) int
sign_bid(_Decimal32 x)
{
	uint32_t b = bits(x);
	return (int32_t)b >> 31U;
}

static inline __attribute__((pure, const)) int
sign_dpd(_Decimal32 x)
{
	uint32_t b = bits(x);
	return (int32_t)b >> 31U;
}

static inline __attribute__((pure, const)) uint_least32_t
mant_bid(_Decimal32 x)
{
	uint32_t b = bits(x);
	register uint_least32_t res;

	if (UNLIKELY((b & 0x60000000U) == 0x60000000U)) {
		/* mantissa is only 21 bits + 0b100 */
		res = 0b100U << 21U;
		res |= (b & 0x1fffffU);
	} else {
		/* mantissa is full */
		res = b & 0x7fffffU;
	}
	return res;
}

static inline __attribute__((pure, const)) uint_least32_t
mant_dpd(_Decimal32 x)
{
	uint32_t b = bits(x);
	register uint_least32_t res;

	/* at least the last 20 bits aye */
	res = b & 0xfffffU;
	b >>= 26U;
	if (UNLIKELY((b & 0b11000U) == 0b11000U)) {
		/* exponent is two more bits, then the high bit */
		res |= (b & 0b00001U | 0b1000U) << 20U;
	} else {
		res |= (b & 0b00111U) << 20U;
	}
	return res;
}

static unsigned int
pack_declet(unsigned int x)
{
/* consider X to be 12bit BCD of 3 digits, return the declet
 * X == BCD(d2, d1, d0) */
	unsigned int res;
	unsigned int d0;
	unsigned int d1;
	unsigned int d2;
	unsigned int c = 0U;

	if (UNLIKELY((d2 = (x >> 8U) & 0xfU) >= 8U)) {
		/* d2 is large, 8 or 9, 0b1000 or 0b1001, store only low bit */
		d2 &= 0x1U;
		c |= 0b100U;
	}

	if (UNLIKELY((d1 = (x >> 4U) & 0xfU) >= 8U)) {
		/* d1 is large, 8 or 9, 0b1000 or 0b1001, store only low bit */
		d1 &= 0x1U;
		c |= 0b010U;
	}

	if (UNLIKELY((d0 = (x >> 0U) & 0xfU) >= 8U)) {
		/* d0 is large, 8 or 9, 0b1000 or 0b1001, store only low bit */
		d0 &= 0x1U;
		c |= 0b001U;
	}

	/* generally the low bits in the large case coincide */
	res = (d2 << 7U) | (d1 << 4U) | d0;

	switch (c) {
	case 0b000U:
		break;
	case 0b001U:
		/* we need d2d1(100)d0 */
		res |= 0b100U << 1U;
		break;
	case 0b010U:
		/* this is d2(gh)d1(101)(i) for d0 = ghi */
		res |= (0b100U << 1U) | ((d0 & 0b110U) << 4U);
		break;
	case 0b011U:
		/* this goes to d2(10)d1(111)d0 */
		res |= (0b100U << 4U) | (0b111U << 1U);
		break;
	case 0b100U:
		/* this goes to (gh)d2d1(110)(i) for d0 = ghi */
		res |= ((d0 & 0b110U) << 7U) | (0b110U << 1U);
		break;
	case 0b101U:
		/* this will be (de)d2(01)f(111)d0 for d1 = def */
		res |= ((d1 & 0b110U) << 7U) | (0b010U << 4U) | (0b111U << 1U);
		break;
	case 0b110U:
		/* goes to (gh)d2(00)d1(111)(i) for d0 = ghi */
		res |= ((d0 & 0b110U) << 7U) | (0b111U << 1U);
		break;
	case 0b111U:
		/* goes to (xx)d2(11)d1(111)d0 */
		res |= (0b110U << 04U) | (0b111U << 1U);
		break;
	default:
		res = 0U;
		break;
	}
	return res;
}

static unsigned int
unpack_declet(unsigned int x)
{
/* go from dpd to bcd, here's the dpd box again: 
 * abc def 0ghi
 * abc def 100i
 * abc ghf 101i
 * ghc def 110i
 * abc 10f 111i
 * dec 01f 111i
 * ghc 00f 111i
 * ??c 11f 111i
 */
	unsigned int res = 0U;

	/* check for the easiest case first */
	if (!(x & 0b1000U)) {
		goto trivial;
	} else {
		switch ((x & 0b1110U) >> 1U) {
		case 0b100U:
		trivial:
			res |= x & 0b1111U;
			res |= (x & 0b1110000U);
			res |= (x & 0b1110000000U) << 1U;
			break;
		case 0b101U:
			/* ghf -> 100f ghi */
			res |= (x & 0b1110000000U) << 1U;
			res |= (0b1000U << 4U) | (x & 0b0010000U);
			res |= ((x & 0b1100000U) >> 4U) | (x & 0b1U);
			break;
		case 0b110U:
			/* ghc -> 100c ghi */
			res |= (0b1000U << 8U) | ((x & 0b0010000000U) << 1U);
			res |= (x & 0b1110000U);
			res |= ((x & 0b1100000000U) >> 7U) | (x & 0b1U);
			break;
		case 0b111U:
			/* grrr */
			switch ((x & 0b1100000U) >> 5U) {
			case 0b10U:
				res |= (0b1110000000U << 1U);
				res |= (0b1000U << 4U) | (x & 0b0000010000U);
				res |= (0b1000U << 0U) | (x & 0b0000000001U);
				break;
			case 0b01U:
				res |= (0b1000U << 8U) |
					((x & 0b0010000000U) << 1U);
				res |= ((x & 0b1100000000U) >> 3U) |
					(x & 0b0000010000U);
				res |= (0b1000U << 0U) | (x & 0b0000000001U);
				break;
			case 0b00U:
				res |= (0b1000U << 8U) |
					((x & 0b0010000000U) << 1U);
				res |= (0b1000U << 4U) | (x & 0b0000010000U);
				res |= ((x & 0b1100000000U) >> 7U) | (x & 0b1U);
				break;
			case 0b11U:
				res |= (0b1000U << 8U) |
					((x & 0b0010000000U) << 1U);
				res |= (0b1000U << 4U) | (x & 0b0000010000U);
				res |= (0b1000U << 0U) | (x & 0b0000000001U);
				break;
			}
			break;
		}
	}
	return res;
}

static uint_least32_t
round_bcd32(uint_least32_t mant, int roff)
{
	register uint_least32_t x;
	unsigned int sh = 0U;

	while (UNLIKELY((x = (mant & 0b1111U) + roff) > 9)) {
		mant >>= 4U;
		sh += 4U;
	}
	mant = (mant & ~0b1111U) | x;
	mant <<= sh;
	return mant;
}

static bcd32_t
strtobcd32(const char *src, char **on)
{
	const char *sp = src;
	uint_least32_t mant = 0U;
	int expo = 0;
	int sign = 0;
	int roff = 0;
	unsigned int nd;

	if (UNLIKELY(*sp == '-')) {
		sign = 1;
		sp++;
	}
	/* skip leading zeros innit? */
	for (; *sp == '0'; sp++);
	/* pick up some digits, not more than 7 though */
	for (nd = 7U; *sp >= '0' && *sp <= '9' && nd > 0; sp++, nd--) {
		mant <<= 4U;
		mant |= U(*sp);
	}
	/* just pick up digits, don't fiddle with the mantissa though
	 * here we determine the round off digit really */
	roff += (*sp >= '5' && *sp <= '9');
	for (; *sp >= '0' && *sp <= '9'; sp++, expo++);
	if (*sp == '.' && nd > 0) {
		/* less than 7 digits, read more from the right side */
		for (sp++;
		     *sp >= '0' && *sp <= '9' && nd > 0; sp++, expo--, nd--) {
			mant <<= 4U;
			mant |= U(*sp);
		}
		/* pick up trailing digits for the word */
		roff += (*sp >= '5' && *sp <= '9');
		for (; *sp >= '0' && *sp <= '9'; sp++);
	} else if (*sp == '.') {
		/* more than 7 digits already, just consume */
		roff += (*sp >= '5' && *sp <= '9');
		for (sp++; *sp >= '0' && *sp <= '9'; sp++);
	}

	if (UNLIKELY(roff)) {
		mant = round_bcd32(mant, sign ? -1 : 1);
	}

	if (LIKELY(on != NULL)) {
		*on = deconst(sp);
	}
	return (bcd32_t){mant, expo, sign};
}

static size_t
bcd32tostr(char *restrict buf, size_t bsz, uint_least32_t mant, int e, int s)
{
	char *restrict bp = buf;
	const char *const ep = buf + bsz;

	/* write the right-of-point side first */
	for (; e < 0 && bp < ep; e++, mant >>= 4U) {
		*bp++ = C(mant & 0b1111U);
	}
	/* write point now */
	if (bp > buf && bp < ep) {
		*bp++ = '.';
	}
	/* write trailing 0s for left-of-point side */
	for (; e > 0 && bp < ep; e--) {
		*bp++ = '0';
	}
	/* now write the rest of the mantissa */
	if (LIKELY(mant)) {
		for (; mant && bp < ep; mant >>= 4U) {
			*bp++ = C(mant & 0b1111U);
		}
	} else if (bp < ep) {
		/* put a leading 0 */
		*bp++ = '0';
	}
	if (s && bp < ep) {
		*bp++ = '-';
	}
	if (bp < ep) {
		*bp = '\0';
	}
	/* reverse the string */
	for (char *ip = buf, *jp = bp - 1; ip < jp; ip++, jp--) {
		char tmp = *ip;
		*ip = *jp;
		*jp = tmp;
	}
	return bp - buf;
}


#if defined HAVE_DFP754_BID_LITERALS
static _Decimal32
quantizebid32(_Decimal32 x, _Decimal32 r)
{
/* d32s look like s??eeeeee mm..23..mm
 * and the decimal is (-1 * s) * m * 10^(e - 101),
 * this implementation is very minimal serving only the cattle use cases */
	int er;
	int ex;
	uint_least32_t m;

	/* get the exponents of x and r */
	er = quantexpbid32(r);
	ex = quantexpbid32(x);

	m = mant_bid(x);

	/* truncate */
	for (; ex < er; ex++) {
		m = m / 10U + ((m % 10U) >= 5U);
	}
	/* expand (only if we don't exceed the range) */
	for (; m < 1000000U && ex > er; ex--) {
		m *= 10U;
	}

	/* assemble the d32 */
	with (uint32_t u = sign_bid(x) << 31U) {
		/* check if 24th bit of mantissa is set */
		if (UNLIKELY(m & (1U << 23U))) {
			u |= 0b11U << 29U;
			u |= (unsigned int)(ex + 101) << 21U;
			/* just use 21 bits of the mantissa */
			m &= 0x1fffffU;
		} else {
			u |= (unsigned int)(ex + 101) << 23U;
			/* use all 23 bits */
			m &= 0x7fffffU;
		}
		u |= m;
		x = bobs(u);
	}
	return x;
}
#elif defined HAVE_DFP754_DPD_LITERALS
static _Decimal32
quantizedpd32(_Decimal32 x, _Decimal32 r)
{
/* d32s dpds look like s??ttteeeeee mm..20..mm
 * this implementation is very minimal serving only the cattle use cases */
	int er;
	int ex;
	uint_least32_t m;
	uint_least32_t b;

	/* get the exponents of x and r */
	er = quantexpdpd32(r);
	ex = quantexpdpd32(x);

	/* get the mantissa TTT MH ML, with MH, ML being declets */
	m = mant_bid(x);
	/* unpack the declets and TTT, TTT first, then MH, then ML */
	b = (m & 0xf00000U);
	b <<= 12U;
	b |= unpack_declet((m >> 10U) & 0x3ffU);
	b <<= 12U;
	b |= unpack_declet((m >> 0U) & 0x3ffU);

	/* truncate (on bcd) */
	for (; ex < er; ex++) {
		b = (b >> 4U) + ((b & 0xfU) >= 5U);
	}
	/* the lowest 4 bits could be 0xa, so flip them over to 0 */
	with (size_t i) {
		for (i = 0U; (b & 0xfU) >= 10U; b >>= 4U, i++);
		/* fix them up by shifting by 4i */
		b <<= 4U * i;
	}

	/* expand (only if we don't exceed the range) */
	for (; b < 0x1000000U && ex > er; ex--) {
		b <<= 4U;
	}

	/* assemble the d32 */
	with (uint32_t u = sign_bid(x) << 31U) {
		u |= pack_declet(b & 0xfffU);
		b >>= 12U;
		u |= pack_declet(b & 0xfffU) << 10U;
		if (UNLIKELY((b >>= 12U) >= 8U)) {
			u |= 0b11U << 29U;
			u |= (unsigned int)(ex + 101) << 21U;
		} else {
			u |= (unsigned int)(ex + 101) << 23U;
		}
		u |= (ex & 0b00111111U) << 20U;
		u |= (b & 0x7U) << 26;
		x = bobs(u);
	}
	return x;
}
#endif	/* HAVE_DFP754_*_LITERALS */

#if !defined HAVE_SCALBND32
# if defined HAVE_DFP754_BID_LITERALS
static _Decimal32
scalbnbid32(_Decimal32 x, int n)
{
	/* just fiddle with the exponent of X then */
	with (uint32_t b = bits(x), u) {
		/* the idea is to xor the current expo with the new expo
		 * and shift the result to the right position and xor again */
		if (UNLIKELY((b & 0x60000000U) == 0x60000000U)) {
			/* 24th bit of mantissa is set, special expo */
			u = (b >> 21U) & 0xffU;
			u ^= u + n;
			u &= 0xffU;
			u <<= 21U;
		} else {
			u = (b >> 23U) & 0xffU;
			u ^= u + n;
			u &= 0xffU;
			u <<= 23U;
		}
		b ^= u;
		x = bobs(b);
	}
	return x;
}
# elif defined HAVE_DFP754_DPD_LITERALS
static _Decimal32
scalbndpd32(_Decimal32 x, int n)
{
	/* just fiddle with the exponent of X then */
	with (uint32_t b = bits(x), u) {
		/* the idea is to xor the current expo with the new expo
		 * and shift the result to the right position and xor again */
		if (UNLIKELY((b & 0x60000000U) == 0x60000000U)) {
			/* 24th bit of mantissa is set, special expo
			 * 11ee T (ee)eeeeee mmm... */
			u = (b >> 20U) & 0x3fU;
			u |= (b >> 21U) & 0xc0U;
			u ^= u + n;
			/* move the top-2 bits out by 1 bit again */
			u = (u & 0x3fU) | ((u & 0xc0U) << 1U);
			u <<= 20U;
		} else {
			/* ee TTT (ee)eeeeee mmm... */
			u = (b >> 20U) & 0x3fU;
			u |= (b >> 23U) & 0xc0U;
			u ^= u + n;
			/* move the top-2 bits out by 3 bits again */
			u = (u & 0x3fU) | ((u & 0xc0U) << 3U);
			u <<= 20U;
		}
		b ^= u;
		x = bobs(b);
	}
	return x;
}
# endif	/* HAVE_DFP754_*_LITERALS */
#endif	/* !HAVE_SCALBND32 */


_Decimal32
strtobid32(const char *src, char **on)
{
/* d32s look like s??eeeeee mm..23..mm
 * and the decimal is (-1 * s) * m * 10^(e - 101),
 * this implementation is very minimal serving only the cattle use cases */
	bcd32_t b = strtobcd32(src, on);
	uint_least32_t mant = 0U;
	_Decimal32 res;

	/* massage the mantissa, first mirror it, so the most significant
	 * nibble is the lowest */
	b.mant = (b.mant & 0xffff0000U) >> 16U | (b.mant & 0x0000ffffU) << 16U;
	b.mant = (b.mant & 0xff00ff00U) >> 8U | (b.mant & 0x00ff00ffU) << 8U;
	b.mant = (b.mant & 0xf0f0f0f0U) >> 4U | (b.mant & 0x0f0f0f0fU) << 4U;
	b.mant >>= 4U;
	for (size_t i = 7U; i > 0U; b.mant >>= 4U, i--) {
		mant *= 10U;
		mant += b.mant & 0b1111U;
	}

	/* assemble the d32 */
	with (uint32_t u) {
		u = b.sign << 31U;

		/* check if 24th bit of mantissa is set */
		if (UNLIKELY(mant & (1U << 23U))) {
			u |= 0b11U << 29U;
			u |= (unsigned int)(b.expo + 101) << 21U;
			/* just use 21 bits of the mantissa */
			mant &= 0x1fffffU;
		} else {
			u |= (unsigned int)(b.expo + 101) << 23U;
			/* use all 23 bits */
			mant &= 0x7fffffU;
		}
		u |= mant;
		res = bobs(u);
	}
	return res;
}

_Decimal32
strtodpd32(const char *src, char **on)
{
/* d32s look like s??eeeeee mm..23..mm
 * and the decimal is (-1 * s) * m * 10^(e - 101),
 * this implementation is very minimal serving only the cattle use cases */
	bcd32_t b = strtobcd32(src, on);
	_Decimal32 res;

	/* pack the mantissa */
	with (uint32_t u) {
		/* lower 3 digits */
		uint_least32_t l3 = pack_declet(b.mant & 0xfffU);
		uint_least32_t u3 = pack_declet((b.mant & 0xfff000U) >> 12U);
		uint_least32_t u7 = (b.mant >> 24U) & 0xfU;
		unsigned int rexp = b.expo + 101;

		/* assemble the d32 */
		u = b.sign << 31U;
		/* check if bits 24-27 (d7) is small or large */
		if (UNLIKELY(u7 >= 8U)) {
			u |= 0b11U << 29U;
			u |= (rexp & 0b11000000U) << 21U;
		} else {
			u |= (rexp & 0b11000000U) << 23U;
		}
		/* the bottom rexp bits */
		u |= (rexp & 0b00111111U) << 20U;
		/* now comes u7 */
		u |= (u7 & 0x7U) << 26U;
		u |= u3 << 10U;
		u |= l3 << 0U;
		res = bobs(u);
	}
	return res;
}

#if !defined HAVE_STRTOD32
_Decimal32
strtod32(const char *src, char **on)
{
# if defined HAVE_DFP754_BID_LITERALS
	return strtobid32(src, on);
# elif defined HAVE_DFP754_DPD_LITERALS
	return strtodpd32(src, on);
# endif	 /* HAVE_DFP754_*_LITERALS */
}
#endif	/* !HAVE_STRTOD32 */

int
bid32tostr(char *restrict buf, size_t UNUSED(bsz), _Decimal32 x)
{
/* d32s look like s??eeeeee mm..23..mm
 * and the decimal is (-1 * s) * m * 10^(e - 101),
 * this implementation is very minimal serving only the cattle use cases */
	int e;
	int s;
	uint_least32_t m;

	/* get the exponent, sign and mantissa */
	e = quantexpbid32(x);
	m = mant_bid(x);
	s = m ? sign_bid(x) : 0/*no stinking signed naughts*/;

	/* reencode m as bcd */
	with (uint_least32_t bcdm = 0U) {
		for (size_t i = 0; i < 7U; i++, bcdm >>= 4U) {
			uint_least32_t digit;

			digit = m % 10U, m /= 10U;
			bcdm |= digit << 28U;
		}
		m = bcdm;
	}
	return (int)bcd32tostr(buf, bsz, m, e, s);
}

int
dpd32tostr(char *restrict buf, size_t UNUSED(bsz), _Decimal32 x)
{
/* d32s look like s??eeeeee mm..23..mm
 * and the decimal is (-1 * s) * m * 10^(e - 101),
 * this implementation is very minimal serving only the cattle use cases */
	int e;
	int s;
	uint_least32_t m;

	/* get the exponent, sign and mantissa */
	e = quantexpdpd32(x);
	if (LIKELY((m = mant_dpd(x)))) {
		uint_least32_t b;

		s = sign_dpd(x);
		/* get us a proper bcd version of M */
		b = (m & 0xf00000U);
		b <<= 12U;
		b |= unpack_declet((m >> 10U) & 0x3ffU);
		b <<= 12U;
		b |= unpack_declet((m >> 0U) & 0x3ffU);
		m = b;
	} else {
		/* no stinking signed 0s and m is in bcd form already */
		s = 0;
	}
	return bcd32tostr(buf, bsz, m, e, s);
}

int
d32tostr(char *restrict buf, size_t bsz, _Decimal32 x)
{
#if defined HAVE_DFP754_BID_LITERALS
	return bid32tostr(buf, bsz, x);
#elif defined HAVE_DFP754_DPD_LITERALS
	return dpd32tostr(buf, bsz, x);
#endif	 /* HAVE_DFP754_*_LITERALS */
}

/* always use our own version,
 * the official version would return NAN in case the significand's
 * capacity is exceeded, we do fuckall in that case. */
_Decimal32
quantized32(_Decimal32 x, _Decimal32 r)
{
#if defined HAVE_DFP754_BID_LITERALS
	return quantizebid32(x, r);
#elif defined HAVE_DFP754_DPD_LITERALS
	/* this one's missing */
	return quantizedpd32(x, r);
#endif	 /* HAVE_DFP754_*_LITERALS */
}

#if !defined HAVE_SCALBND32
_Decimal32
scalbnd32(_Decimal32 x, int n)
{
# if defined HAVE_DFP754_BID_LITERALS
	return scalbnbid32(x, n);
# elif defined HAVE_DFP754_DPD_LITERALS
	return scalbndpd32(x, n);
# endif	 /* HAVE_DFP754_*_LITERALS */
}
#endif	/* !HAVE_SCALBND32 */

/* truf-dfp754.c ends here */
