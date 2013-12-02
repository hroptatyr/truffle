/*** step.c -- time-symbol-exposure-price hash tables
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
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
#include <string.h>
#include "step.h"
#include "truf-dfp754.h"
#include "nifty.h"

/* the beef table, slot 0-63 are in stkstk[0U], slots 64-127 in stkstk[1U],
 * slots 128-256 in stkstk[2U], etc. */
static struct truf_step_s inistk[64U];
static truf_step_t stkstk[64U] = {inistk};
/* alloc size, index of next stkstk */
static size_t zstk;
/* number of elements */
static size_t nstk;

static inline __attribute__((always_inline)) unsigned int
fls(unsigned int x)
{
	return x ? sizeof(x) * 8U - __builtin_clz(x) : 0U;
}

static inline struct stk_off_s {
	/* which stack */
	size_t stk;
	/* which cell */
	size_t cel;
} decomp(size_t x)
{
	size_t log2 = fls(x);

	if (log2-- <= 6U) {
		return (struct stk_off_s){0U, x};
	}
	return (struct stk_off_s){log2 - 5U, x % (1U << log2)};
}

static inline struct stk_off_s
get_off(size_t idx, size_t mod)
{
	/* no need to negate MOD as it's a 2-power */
	return decomp(-idx % mod);
}

static void*
recalloc(void *buf, size_t nmemb_ol, size_t nmemb_nu, size_t membz)
{
	nmemb_ol *= membz;
	nmemb_nu *= membz;
	buf = realloc(buf, nmemb_nu);
	memset((uint8_t*)buf + nmemb_ol, 0, nmemb_nu - nmemb_ol);
	return buf;
}

static void
init(struct stk_off_s o, truf_sym_t sym)
{
	stkstk[o.stk][o.cel].sym = sym;
	/* no prices yet */
	stkstk[o.stk][o.cel].bid = stkstk[o.stk][o.cel].ask = nand32(NULL);
	/* no exposures either */
	stkstk[o.stk][o.cel].old = stkstk[o.stk][o.cel].new = 0.df;
	return;
}


truf_step_t
truf_step_find(truf_sym_t sym)
{
	size_t hx = truf_sym_hx(sym);
	size_t mod = 64U;

	while (1) {
		/* just try what we've got */
		for (; mod <= (64U << zstk); mod *= 2U) {
			struct stk_off_s o = get_off(hx, mod);

			if (UNLIKELY(!sym.u) ||
			    LIKELY(stkstk[o.stk][o.cel].sym.u == sym.u)) {
				/* found him */
				return stkstk[o.stk] + o.cel;
			} else if (stkstk[o.stk][o.cel].sym.u == 0U) {
				/* found empty slot */
				init(o, sym);
				nstk++;
				return stkstk[o.stk] + o.cel;
			}
		}
		/* quite a lot of collisions, resize then */
		with (size_t sz = (64U << zstk)) {
			stkstk[++zstk] = calloc(sz, sizeof(**stkstk));
		}
	}
}

truf_step_t
truf_step_iter(void)
{
/* coroutine with static storage */
	static size_t s;
	static size_t i;

	if (s == 0U) {
		while (i < 64U) {
			size_t this = i++;
			if (stkstk[s][this].sym.u) {
				return stkstk[s] + this;
			}
		}
		s++;
		i = 0U;
	}
	while (s <= zstk) {
		while (i < (64U << (s - 1U))) {
			size_t this = i++;
			if (stkstk[s][this].sym.u) {
				return stkstk[s] + this;
			}
		}
		s++;
		i = 0U;
	}
	/* reset offsets for next iteration */
	s = 0U;
	return NULL;
}

void
truf_init_step(void)
{
	nstk = 0U;
	zstk = 0U;

	/* initialise the first slot for special NOSYM symbol */
	init((struct stk_off_s){0U, 0U}, (truf_sym_t){0U});
	return;
}

void
truf_fini_step(void)
{
	for (size_t i = 1U; i <= zstk; i++) {
		free(stkstk[i]);
	}
	zstk = 0U;
	nstk = 0U;
	return;
}

/* step.c ends here */
