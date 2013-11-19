/*** sym.c -- symbology
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
#include <string.h>
#include "sym.h"
#include "nifty.h"


truf_sym_t
truf_sym_rd(const char *str, char **on)
{
	truf_sym_t res;
	char *tmp;

	if (res.mmy = truf_mmy_rd(str, &tmp)) {
		/* good, it's a mmy, job done */
		;
	} else if (res.str = truf_str_rd(str, &tmp)) {
		/* ah, it's a str, tick */
		;
	} else {
		tmp = deconst(str);
		res.u = 0U;
	}
	if (LIKELY(on != NULL)) {
		*on = tmp;
	}
	return res;
}

size_t
truf_sym_wr(char *restrict buf, size_t bsz, truf_sym_t sym)
{
	if (truf_mmy_p(sym)) {
		return truf_mmy_wr(buf, bsz, sym.mmy);
	} else if (truf_str_p(sym)) {
		return truf_str_wr(buf, bsz, sym.str);
	}
	return 0U;
}

void
truf_init_sym(void)
{
	truf_init_str();
	return;
}

void
truf_fini_sym(void)
{
	truf_fini_str();
	return;
}

/* sym.c ends here */
