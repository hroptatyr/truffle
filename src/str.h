/*** str.h -- pools of strings (for symbology)
 *
 * Copyright (C) 2013-2020 Sebastian Freundt
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
#if !defined INCLUDED_str_h_
#define INCLUDED_str_h_

#include <stdlib.h>
#include <stdint.h>

/**
 * truf_strs are length+offset integers, 32 bits wide, always even.
 *
 * LLLLLLLL OOOOOOOOOOOOOOOOOOOOOO00
 * ^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^||
 * length   offset                divisible by 4 
 **/
typedef uint_fast32_t truf_str_t;

/**
 * Intern the string STR of length LEN. */
extern truf_str_t truf_str_intern(const char *str, size_t len);

/**
 * Unintern the str object. */
extern void truf_str_unintern(truf_str_t);

/**
 * Intern the string STR up to the next word boundary. */
extern truf_str_t truf_str_rd(const char *str, char **on);

/**
 * Output the interned S into BUF of size BSZ, return bytes written. */
extern size_t truf_str_wr(char *restrict buf, size_t bsz, truf_str_t s);

extern void truf_init_str(void);
extern void truf_fini_str(void);

#endif	/* INCLUDED_str_h_ */
