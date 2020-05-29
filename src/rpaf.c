/*** rpaf.c -- reference prices and accrued flows
 *
 * Copyright (C) 2009-2020 Sebastian Freundt
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
#include <assert.h>
#if defined HAVE_DFP754_H
# include <dfp754.h>
#elif defined HAVE_DFP_STDLIB_H
# include <dfp/stdlib.h>
#else  /* !HAVE_DFP754_H && !HAVE_DFP_STDLIB_H */
extern int isinfd64(_Decimal64);
#endif	/* HAVE_DFP754_H */
#include "truffle.h"
#include "rpaf.h"
#include "nifty.h"

typedef struct {
	truf_sym_t sym;
	truf_rpaf_t rpaf[1U];
} *srpaf_t;

/* the beef table */
static srpaf_t rstk;
/* alloc size, 2-power */
static size_t zstk;
/* number of elements */
static size_t nstk;

static const truf_rpaf_t truf_nul_rpaf = {ZEROPX, ZEROPX};

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

static srpaf_t
truf_rpaf_find(truf_sym_t sym)
{
	size_t hx = truf_sym_hx(sym);

	while (1) {
		/* just try what we've got */
		for (size_t mod = 64U; mod <= zstk; mod *= 2U) {
			size_t off = get_off(hx, mod);

			if (LIKELY(rstk[off].sym.u == sym.u)) {
				/* found him */
				return rstk + off;
			} else if (rstk[off].sym.u == 0U) {
				/* found empty slot */
				rstk[off].sym = sym;
				/* init the rpaf cell */
				rstk[off].rpaf->refprc = NANPX;
				rstk[off].rpaf->cruflo = ZEROPX;
				nstk++;
				return rstk + off;
			}
		}
		/* quite a lot of collisions, resize then */
		rstk = recalloc(rstk, zstk, 2U * zstk, sizeof(*rstk));
		zstk *= 2U;
	}
}

static void
rpaf_scru(srpaf_t sr, const struct truf_step_s st[static 1U])
{
	truf_rpaf_t *r = sr->rpaf;
	truf_price_t bid = st->bid;
	truf_price_t ask = isnanpx(st->ask) ? bid : st->ask;
	truf_price_t ref;
	truf_expos_t edif;

	/* business logic here:
	 * we replace the old reference price by the new
	 * the difference between the two is weighted by the exposure and
	 * returned in the cruflo slot
	 * this routine is for callers who want to do the summing themselves */
	if (isnanpx(r->refprc)) {
		/* just set the reference price */
		if (st->new < ZEROEX) {
			/* going short or there's just one price */
			r->refprc = bid;
		} else if (st->new > ZEROEX) {
			r->refprc = ask;
		}
		/* mimic a 0 flow but with the right quantum */
		r->cruflo = scalbnd(ZEROPX, quantexpd(st->new));
	} else if ((edif = st->new - st->old) != ZEROEX) {
		/* we determined it's an edge */

		if (edif < ZEROEX) {
			/* short/close */
			ref = bid;
		} else if (edif > ZEROEX) {
			/* long/open */
			ref = ask;
		} else {
			assert(0);
		}

		/* the level formula is quite simple diff_prc * diff_exp */
		r->cruflo += (r->refprc - ref) * edif;
		if (st->new == ZEROEX) {
			/* reset refprc */
			r->refprc = NANPX;
		} else {
			r->refprc = ref;
		}
	} else {
		/* it's a level, i.e. operate in settlement mode */
		if (st->new > ZEROEX) {
			ref = bid;
		} else {
			ref = ask;
		}

		r->cruflo += (ref - r->refprc) * st->new;
		r->refprc = ref;
	}
	return;
}


truf_rpaf_t
truf_rpaf_step(const struct truf_step_s st[static 1U])
{
	srpaf_t sr;

	if (UNLIKELY((sr = truf_rpaf_find(st->sym)) == NULL)) {
		return truf_nul_rpaf;
	}
	/* otherwise init rpaf */
	sr->rpaf->cruflo = ZEROPX;
	rpaf_scru(sr, st);
	return *sr->rpaf;
}

truf_rpaf_t
truf_rpaf_scru(const struct truf_step_s st[static 1U])
{
	srpaf_t sr;

	if (UNLIKELY((sr = truf_rpaf_find(st->sym)) == NULL)) {
		return truf_nul_rpaf;
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
	return;
}

void
truf_fini_rpaf(void)
{
	if (LIKELY(rstk != NULL)) {
		free(rstk);
	}
	rstk = NULL;
	zstk = 0U;
	nstk = 0U;
	return;
}

/* rpaf.c ends here */
