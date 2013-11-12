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
#include "mmy.h"
#include "nifty.h"

/* a hash is the bucket locator and a chksum for collision detection */
typedef struct {
	size_t idx;
	uint32_t chk;
} hash_t;

/* the beef table */
static truf_step_t sstk;
/* checksum table */
static uint32_t *schk;
/* alloc size, 2-power */
static size_t zstk;
/* number of elements */
static size_t nstk;

static hash_t
murmur(const uint8_t *str, size_t ssz)
{
/* tokyocabinet's hasher */
	size_t idx = 19780211U;
	uint32_t hash = 751U;
	const uint8_t *rp = str + ssz;

	while (ssz--) {
		idx = idx * 37U + *str++;
		hash = (hash * 31U) ^ *--rp;
	}
	return (hash_t){idx, hash};
}

static hash_t
hx_mmy(truf_mmy_t ym)
{
	size_t idx = 19780211U;

	idx += 983U * truf_mmy_year(ym);
	idx += 991U * truf_mmy_mon(ym);
	idx += 997U * truf_mmy_day(ym);
	return (hash_t){idx, ym};
}

static inline size_t
get_off(size_t idx, size_t mod)
{
	return mod - (idx % mod) - 1U;
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


truf_step_t
truf_step_find(truf_sym_t sym)
{
	hash_t hx;

	if (truf_mmy_p(sym)) {
		/* hash differently */
		hx = hx_mmy(sym);
	} else {
		size_t ssz = strlen((const char*)sym);
		hx = murmur((const uint8_t*)sym, ssz);
	}

	while (1) {
		/* just try what we've got */
		for (size_t mod = 64U; mod <= zstk; mod *= 2U) {
			size_t off = get_off(hx.idx, mod);

			if (LIKELY(schk[off] == hx.chk)) {
				/* found him */
				return sstk + off;
			} else if (sstk[off].sym == 0U) {
				/* found empty slot */
				schk[off] = hx.chk;
				sstk[off].sym = sym;
				nstk++;
				return sstk + off;
			}
		}
		/* quite a lot of collisions, resize then */
		sstk = recalloc(sstk, zstk, 2U * zstk, sizeof(*sstk));
		schk = recalloc(schk, zstk, 2U * zstk, sizeof(*schk));
		zstk *= 2U;
	}
}

truf_step_t
truf_step_iter(void)
{
/* coroutine with static storage */
	static size_t i;

	while (i < zstk) {
		size_t this = i++;
		if (sstk[this].sym != 0U) {
			return sstk + this;
		}
	}
	/* reset offset for next iteration */
	i = 0U;
	return NULL;
}

void
truf_init_step(void)
{
	nstk = 0U;
	zstk = 64U;
	sstk = calloc(zstk, sizeof(*sstk));
	schk = calloc(zstk, sizeof(*schk));
	return;
}

void
truf_fini_step(void)
{
	if (LIKELY(sstk != NULL)) {
		free(sstk);
	}
	if (LIKELY(schk != NULL)) {
		free(schk);
	}
	sstk = NULL;
	schk = NULL;
	zstk = 0U;
	nstk = 0U;
	return;
}

/* step.c ends here */
