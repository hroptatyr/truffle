/*** str.c -- pools of strings (for symbology)
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdint.h>
#include <string.h>
#include "str.h"
#include "nifty.h"

/* a hash is the bucket locator and a chksum for collision detection */
typedef struct {
	size_t idx;
	uint_fast32_t chk;
} hash_t;

/* the beef table */
static struct {
	truf_str_t str;
	uint_fast32_t chk;
} *sstk;
/* alloc size, 2-power */
static size_t zstk;
/* number of elements */
static size_t nstk;

/* the big string obarray */
static char *obs;
/* alloc size, 2-power */
static size_t obz;
/* next ob */
static size_t obn;


static hash_t
murmur(const uint8_t *str, size_t len)
{
/* tokyocabinet's hasher */
	size_t idx = 19780211U;
	uint_fast32_t hash = 751U;
	const uint8_t *rp = str + len;

	while (len--) {
		idx = idx * 37U + *str++;
		hash = (hash * 31U) ^ *--rp;
	}
	return (hash_t){idx, hash};
}

static inline size_t
get_off(size_t idx, size_t mod)
{
	/* no need to negate MOD as it's a 2-power */
	return -idx % mod;
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

static truf_str_t
make_str(const char *str, size_t len)
{
/* put STR (of length LEN) into string obarray, don't check for dups */
	/* make sure we pad with \0 bytes to the next 4-byte multiple */
	size_t pad = ((len / 4U) + 1U) * 4U;
	truf_str_t res;

	if (UNLIKELY(obn + pad >= obz)) {
		size_t nuz = ((obn + pad) / 1024U + 1U) * 1024U;
		if (nuz < 2U * obz) {
			nuz = 2U * obz;
		}
		obs = recalloc(obs, obz, nuz, sizeof(*obs));
		obz = nuz;
	}
	/* paste the string in question */
	memcpy(obs + obn, str, len);
	/* assemble the result */
	res = len << 24U | obn;
	/* inc the obn pointer */
	obn += pad;
	return res;
}


/* public API */
truf_str_t
truf_str_intern(const char *str, size_t len)
{
#define SSTK_MINZ	(256U)
	hash_t hx = murmur((const uint8_t*)str, len);

	while (1) {
		/* just try what we've got */
		for (size_t mod = SSTK_MINZ; mod <= zstk; mod *= 2U) {
			size_t off = get_off(hx.idx, mod);

			if (LIKELY(sstk[off].chk == hx.chk)) {
				/* found him */
				return sstk[off].str;
			} else if (sstk[off].str == 0U) {
				/* found empty slot */
				truf_str_t sym = make_str(str, len);
				sstk[off].str = sym;
				sstk[off].chk = hx.chk;
				nstk++;
				return sym;
			}
		}
		/* quite a lot of collisions, resize then */
		sstk = recalloc(sstk, zstk, 2U * zstk, sizeof(*sstk));
		zstk *= 2U;
	}
	/* not reached */
}

truf_str_t
truf_str_rd(const char *str, char **on)
{
	static const char sep[] = " \t\n";
	truf_str_t res = 0U;
	size_t len;

	if ((len = strcspn(str, sep)) > 0U) {
		/* very well worth it then, intern the bugger */
		res = truf_str_intern(str, len);
	}
	if (LIKELY(on != NULL)) {
		*on = (char*)deconst(str) + len;
	}
	return res;
}

size_t
truf_str_wr(char *restrict buf, size_t bsz, truf_str_t s)
{
	size_t len = s >> 24U;
	const char *str = obs + (s & 0xffffffU);

	if (UNLIKELY(len >= bsz)) {
		len = bsz - 1U;
	}
	memcpy(buf, str, len + 1U/*for \nul*/);
	return len;
}

void
truf_init_str(void)
{
	nstk = 0U;
	zstk = SSTK_MINZ;
	sstk = calloc(zstk, sizeof(*sstk));
	return;
}

void
truf_fini_str(void)
{
	if (LIKELY(sstk != NULL)) {
		free(sstk);
	}
	sstk = NULL;
	zstk = 0U;
	nstk = 0U;
	return;
}

/* str.c ends here */
