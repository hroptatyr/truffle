/*** truffle.h -- tool to roll-over futures contracts
 *
 * Copyright (C) 2011-2018 Sebastian Freundt
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
#if !defined INCLUDED_truffle_h_
#define INCLUDED_truffle_h_

#include <stdint.h>
#include "dfp754_d32.h"
#include "dfp754_d64.h"
#include "sym.h"

typedef _Decimal32 truf_price_t;
typedef _Decimal64 truf_quant_t;
typedef _Decimal32 truf_expos_t;

#define NANPX	NAND32
#define isnanpx	isnand32
#define NANQX	NAND64
#define isnanqx	isnand64
#define NANEX	NAND32
#define isnanex	isnand32

#define strtopx	strtod32
#define pxtostr	d32tostr
#define strtoqx	strtod64
#define qxtostr	d64tostr
#define strtoex	strtod32
#define extostr	d32tostr

#define ZEROPX	0.df
#define UNITPX	1.df
#define ZEROQX	0.dd
#define UNITQX	1.dd
#define ZEROEX	0.df
#define UNITEX	1.df

#define scalbnd(x, i)				\
	_Generic((x),				\
		 _Decimal32: scalbnd32,		\
		 _Decimal64: scalbnd64,		\
		 default: scalbnd64		\
		)(x, i)
#define quantexpd(x)				\
	_Generic((x),				\
		 _Decimal64: quantexpd64,	\
		 _Decimal32: quantexpd32,	\
		 default: quantexpd64		\
		)(x)
#define quantized(x, s)				\
	_Generic((x),				\
		 _Decimal64: quantized64,	\
		 _Decimal32: quantized32,	\
		 default: quantized64		\
		)(x, s)

#endif	/* INCLUDED_truffle_h_ */
