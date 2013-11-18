/*** trod.c -- convert or generate truffle roll-over description files
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
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "truffle.h"
#include "instant.h"
#include "trod.h"
#include "mmy.h"
#include "truf-dfp754.h"
#include "nifty.h"


/* auxiliaries */
static size_t
xstrlcpy(char *restrict dst, const char *src, size_t dsz)
{
	size_t ssz = strlen(src);
	if (ssz > dsz) {
		ssz = dsz - 1U;
	}
	memcpy(dst, src, ssz);
	dst[ssz] = '\0';
	return ssz;
}


/* new api */
truf_trod_t
truf_trod_rd(const char *str, char **on)
{
	static const char sep[] = " \t\n";
	truf_trod_t res;
	const char *brk;

	switch (*(brk += strcspn(brk = str, sep))) {
	case '\0':
	case '\n':
		/* no separator, so it's just a symbol
		 * imply exp = 0.df if ~FOO, exp = 1.df otherwise */
		if (*str != '~') {
			res.exp = 1.df;
		} else {
			/* could be ~FOO notation */
			res.exp = 0.df;
			str++;
		}
		/* also set ON pointer */
		if (LIKELY(on != NULL)) {
			*on = deconst(brk);
		}
		break;
	default:
		/* get the exposure sorted (hopefully just 1 separator) */
		res.exp = strtod32(brk + 1U, on);
		break;
	}
	/* before blindly strdup()ing the symbol check if it's not by
	 * any chance in MMY notation */
	with (truf_mmy_t ym) {
		char *tmp;

		if ((ym = truf_mmy_rd(str, &tmp))) {
			/* YAY */
			res.sym = ym;
		} else {
			tmp = strndup(str, brk - str);
			res.sym = (uintptr_t)tmp;
		}
	}
	return res;
}

size_t
truf_trod_wr(char *restrict buf, size_t bsz, truf_trod_t t)
{
	char *restrict bp = buf;
	const char *const ep = bp + bsz;

	if (LIKELY(t.sym)) {
		if (truf_mmy_p(t.sym)) {
			bp += truf_mmy_wr(bp, ep - bp, t.sym);
		} else {
			bp += xstrlcpy(bp, (const char*)t.sym, ep - bp);
		}
	}
	if (LIKELY(bp < ep)) {
		*bp++ = '\t';
	}
	bp += d32tostr(bp, ep - bp, t.exp);
	if (LIKELY(bp < ep)) {
		*bp = '\0';
	} else if (LIKELY(ep > bp)) {
		*--bp = '\0';
	}
	return bp - buf;
}

/* trod.c ends here */
