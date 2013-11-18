/*** rpaf.c -- reference prices and accrued flows
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
#include "truffle.h"
#include "rpaf.h"
#include "step.h"
#include "mmy.h"
#include "truf-dfp754.h"
#include "nifty.h"

/* a hash is the bucket locator and a chksum for collision detection */
typedef struct {
	size_t idx;
	uint32_t chk;
} hash_t;

typedef struct {
	truf_sym_t sym;
	truf_rpaf_t rpaf[1U];
} *srpaf_t;

/* the beef table */
static srpaf_t rstk;
/* checksum table */
static uint32_t *rchk;
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

static srpaf_t
truf_rpaf_find(truf_sym_t sym)
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

			if (LIKELY(rchk[off] == hx.chk)) {
				/* found him */
				return rstk + off;
			} else if (rstk[off].sym == 0U) {
				/* found empty slot */
				rchk[off] = hx.chk;
				rstk[off].sym = sym;
				/* init the rpaf cell */
				rstk[off].rpaf->refprc = nand32(NULL);
				rstk[off].rpaf->cruflo = 0.df;
				nstk++;
				return rstk + off;
			}
		}
		/* quite a lot of collisions, resize then */
		rstk = recalloc(rstk, zstk, 2U * zstk, sizeof(*rstk));
		rchk = recalloc(rchk, zstk, 2U * zstk, sizeof(*rchk));
		zstk *= 2U;
	}
}

static void
rpaf_scru(srpaf_t sr, const struct truf_step_s st[static 1U])
{
	truf_rpaf_t *r = sr->rpaf;
	truf_price_t bid = st->bid;
	truf_price_t ask = isnand32(st->ask) ? bid : st->ask;
	truf_expos_t edif;

	/* business logic here:
	 * we replace the old reference price by the new
	 * the difference between the two is weighted by the exposure and
	 * returned in the cruflo slot
	 * this routine is for callers who want to do the summing themselves */
	if (isnand32(r->refprc)) {
		/* just set the reference price */
		if (st->new < 0.df) {
			/* going short or there's just one price */
			r->refprc = bid;
		} else {
			r->refprc = ask;
		}
	} else if ((edif = st->new - st->old) < 0.df) {
		/* short/close */
		r->cruflo += (r->refprc - bid) * edif;
		r->refprc = bid;
	} else if (edif > 0.df) {
		/* long/open */
		r->cruflo += (r->refprc - ask) * edif;
		r->refprc = ask;
	} else {
		/* settle */
		if (st->new > 0.df) {
			r->cruflo += (bid - r->refprc) * st->new;
			r->refprc = bid;
		} else {
			r->cruflo += (ask - r->refprc) * st->new;
			r->refprc = ask;
		}
	}
	return;
}


truf_rpaf_t
truf_rpaf_step(const struct truf_step_s st[static 1U])
{
	srpaf_t sr;

	if (UNLIKELY((sr = truf_rpaf_find(st->sym)) == NULL)) {
		return (truf_rpaf_t){0.df};
	}
	/* otherwise init rpaf */
	sr->rpaf->cruflo = 0.df;
	rpaf_scru(sr, st);
	return *sr->rpaf;
}

truf_rpaf_t
truf_rpaf_scru(const struct truf_step_s st[static 1U])
{
	srpaf_t sr;

	if (UNLIKELY((sr = truf_rpaf_find(st->sym)) == NULL)) {
		return (truf_rpaf_t){0.df};
	}
	/* otherwise init rpaf */
	rpaf_scru(sr, st);
	return *sr->rpaf;
}

void
truf_init_rpaf(void)
{
	nstk = 0U;
	zstk = 64U;
	rstk = calloc(zstk, sizeof(*rstk));
	rchk = calloc(zstk, sizeof(*rchk));
	return;
}

void
truf_fini_rpaf(void)
{
	if (LIKELY(rstk != NULL)) {
		free(rstk);
	}
	if (LIKELY(rchk != NULL)) {
		free(rchk);
	}
	rstk = NULL;
	rchk = NULL;
	zstk = 0U;
	nstk = 0U;
	return;
}

/* rpaf.c ends here */
