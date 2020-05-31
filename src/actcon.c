/*** actcon.c -- active contracts
 *
 * Copyright (C) 2019-2020 Sebastian Freundt
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "actcon.h"
#include "nifty.h"

struct actcon_s*
read_actcon(const char *spec)
{
	struct actcon_s *res;
	unsigned int nsum = 1U;
	const char *sp, *ep;

	for (ep = sp = spec; *ep; nsum += *ep == '+', ep++);
	res = malloc(sizeof(*res) + nsum * sizeof(struct timoof_s));
	res->nsum = nsum;
	res->pool = spec;

	/* start parsing */
	for (size_t j = 0U; j < nsum; j++, sp++) {
		char *tp = NULL;
		long unsigned int n;

		n = strtoul(sp, &tp, 10);
		if (tp == NULL) {
			goto err;
		} else if (*tp == '*') {
			res->sum[j].n = n;
			tp++;
			sp = tp, tp = NULL;
			n = strtoul(sp, &tp, 10);
			if (tp == NULL) {
				goto err;
			} else if (*tp == '/') {
				res->sum[j].m = n;
				tp++;
			} else {
				res->sum[j].m = 0U;
			}
		} else if (*tp == '/') {
			res->sum[j].n = 1U;
			res->sum[j].m = n;
			tp++;
		} else {
			res->sum[j].n = 1U;
			res->sum[j].m = 0U;
		}
		/* see what goes */
		for (n = 0U, sp = tp; *tp >= 'F' && *tp <= 'Z'; tp++);
		res->sum[j].x = sp;
		res->sum[j].l = tp - sp;
		res->sum[j].m = res->sum[j].m ?: res->sum[j].l;
		/* fast forward beyond + */
		for (; *sp && *sp != '+'; sp++);
	}
	return res;
err:
	free(res);
	return NULL;
}

void
free_actcon(struct actcon_s *spec)
{
	free(spec);
	return;
}

void
prnt_actcon(struct actcon_s *spec)
{
	/* spell structure */
	for (size_t j = 0U; j < spec->nsum; j++) {
		printf("%u*%u/%.*s", spec->sum[j].n, spec->sum[j].m, (int)spec->sum[j].l, spec->sum[j].x);
		fputc((j + 1U < spec->nsum) ? '+' : '\n', stdout);
	}
	return;
}

static inline __attribute__((pure)) char
pivot_trans(char c, char cur)
{
	c -= cur;
	c--;
	c = (char)(c >= 0 ? c : c + ' ');
	return c;
}

void
xpnd_actcon(struct actcon_s *spec)
{
	size_t *cidx;
	size_t *cite;
	size_t ncand = 0U;
	char *cand;
	char npiv = '@';
	uint_least32_t months = 0U;

	for (size_t j = 0U; j < spec->nsum; j++) {
		ncand += spec->sum[j].n * spec->sum[j].m;
	}
	cite = malloc(spec->nsum * sizeof(*cidx) * 2 + ncand + 2U * (spec->nsum + 1U));
	/* initialise */
	cidx = cite + spec->nsum;
	cand = (char*)(cite + 2U * spec->nsum);
	memset(cite, 0, sizeof(cite));

	/* 1 year of expansion */
	for (size_t j = 0U; j < spec->nsum; j++) {
		for (size_t k = 0U; k < spec->sum[j].l; k++) {
			months |= 1 << ((spec->sum[j].x[k] - '@') & 0x1fU);
		}
	}
	months = __builtin_popcount(months);

	for (size_t q = 0U; q < months; q++) {
		for (size_t j = 0U, c = 0U; j < spec->nsum; j++) {
			size_t i = cite[j] % spec->sum[j].l;

			cidx[j] = c;
			for (size_t o = 0U, n = spec->sum[j].n; o < n; o++) {
				unsigned int l = spec->sum[j].l;

				for (size_t k = 0U, m = spec->sum[j].m; k < m; k++) {
					cand[c++] = spec->sum[j].x[(i + k) % l];
				}
			}
			cand[c++] = '~';
		}

		/* determine who's going to change state next */
		with (unsigned int next = 0U) {
			char cmin = pivot_trans(cand[cidx[next]], npiv);
			for (size_t j = 1U; j < spec->nsum; j++) {
				if (pivot_trans(cand[cidx[j]], npiv) < cmin) {
					next = j;
					cmin = pivot_trans(cand[cidx[next]], npiv);
				}
			}
			npiv = cand[cidx[next]];
			fputc(npiv, stdout);
			cidx[next]++;
			cite[next]++;
		}
		for (char curr = npiv;;) {
			unsigned int min = 0U;
			char cmin = pivot_trans(cand[cidx[min]], curr);

			/* find the smallest item that is >= curr */
			for (size_t j = 1U; j < spec->nsum; j++) {
				if (pivot_trans(cand[cidx[j]], curr) < cmin) {
					min = j;
					cmin = pivot_trans(cand[cidx[min]], curr);
				}
			}
			if (UNLIKELY((curr = cand[cidx[min]]) == '~')) {
				break;
			}
			fputc(curr, stdout);
			cidx[min]++;
		}
		fputc('\n', stdout);
	}

	free(cite);
	return;
}

/* actcon.c ends here */
