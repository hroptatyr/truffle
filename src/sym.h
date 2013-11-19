/*** sym.h -- symbology
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
#if !defined INCLUDED_sym_h_
#define INCLUDED_sym_h_

#include <stdint.h>
#include "mmy.h"
#include "str.h"

typedef union {
	uint_fast32_t u;
	truf_str_t str;
	truf_mmy_t mmy;
} truf_sym_t;

/**
 * Try and read the string STR as symbol and return a truf sym object. */
extern truf_sym_t truf_sym_rd(const char *str, char **ptr);

/**
 * Output SYM into BUF of size BSZ, return the number of bytes written. */
extern size_t truf_sym_wr(char *restrict buf, size_t bsz, truf_sym_t sym);

/**
 * Like `truf_sym_rd()' but allocate space for a non-MMY symbol. */
extern truf_sym_t truf_sym_rd_alloc(const char *str, char **ptr);


static inline __attribute__((pure, const)) bool
truf_mmy_p(truf_sym_t sym)
{
/* return true if SYM encodes a MMY and false otherwise */
	return (sym.u & 0b1U);
}

static inline __attribute__((pure, const)) bool
truf_str_p(truf_sym_t sym)
{
/* return true if SYM encodes a str object and false otherwise */
	return (sym.u & 0b11U) == 0U;
}

#endif	/* INCLUDED_sym_h_ */
