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
	unsigned int nwrd = 0U;
	const char *sp, *ep;

	for (ep = sp = spec; *ep; nsum += *ep == '+', nwrd += *ep == '|', ep++);
	res = malloc(sizeof(*res) + (nsum += nwrd) * sizeof(struct timoof_s));
	res->nsum = nsum;
	res->nwrd = nwrd;
	res->pool = spec;

	/* start parsing */
	for (size_t j = 0U, w = 0U; j < nsum; j++, w += *sp++ == '|') {
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
			} else if (*tp == '/' || *tp == '<') {
				res->sum[j].m = n << 1U;
				res->sum[j].m ^= *tp == '<';
				tp++;
			} else {
				res->sum[j].m = 0U;
			}
		} else if (*tp == '/' || *tp == '<') {
			res->sum[j].n = 1U;
			res->sum[j].m = n << 1U;
			res->sum[j].m ^= *tp == '<';
			tp++;
		} else {
			res->sum[j].n = 1U;
			res->sum[j].m = 0U;
		}
		/* see what goes */
		for (n = 0U, sp = tp; *tp >= 'F' && *tp <= 'Z'; tp++);
		res->sum[j].x = sp;
		res->sum[j].l = tp - sp;
		res->sum[j].m = ((res->sum[j].m >> 1U) ?: res->sum[j].l) << 1U ^ res->sum[j].m & 0b1U;
		res->sum[j].w = w;
		if (UNLIKELY(tp == sp)) {
			goto err;
		}
		/* fast forward beyond + */
		for (; *sp && *sp != '+' && *sp != '|'; sp++);
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
prnt_actcon(const struct actcon_s *spec)
{
	/* spell structure */
	for (size_t j = 0U; j < spec->nsum; j++) {
		printf("%u*%u/%.*s", spec->sum[j].n, spec->sum[j].m >> 1U, (int)spec->sum[j].l, spec->sum[j].x);
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
xpnd_actcon(const struct actcon_s *spec, char from, char till)
{
/* expand currently active contracts */
	size_t *cidx;
	size_t ncand = 0U;
	char *cand;

	for (size_t j = 0U; j < spec->nsum; j++) {
		ncand += spec->sum[j].n * (spec->sum[j].m >> 1U);
	}
	cidx = malloc(spec->nsum * sizeof(*cidx) + ncand + 2U * (spec->nsum + 1U));
	cand = (char*)(cidx + spec->nsum);

	for (char npiv = --from, prev = '\0'; npiv >= from && npiv < till; prev = npiv) {
		char curp = npiv;

		for (size_t w = 0U, jb = 0U, je; w <= spec->nwrd; w++, jb = je) {
			for (je = jb; je < spec->nsum && spec->sum[je].w == w; je++);

			/* get a candidate from every summand */
			for (size_t j = jb, c = jb; j < je; j++) {
				unsigned int l = spec->sum[j].l;
				size_t i;

				for (i = 0U; i < l && spec->sum[j].x[i] <= curp; i++);

				cidx[j] = c;
				for (size_t o = 0U, n = spec->sum[j].n; o < n; o++) {
					size_t k;
					const size_t m = spec->sum[j].m >> 1U;

					if (spec->sum[j].m & 0b1 && i < m) {
						for (k = i; k < m; k++) {
							cand[c++] = spec->sum[j].x[k % l];
						}
					} else {
						for (k = 0U; k < m; k++) {
							cand[c++] = spec->sum[j].x[(i + k) % l];
						}
					}
				}
				cand[c++] = '~';
			}

			/* determine who's going to change state next */
			with (size_t next = jb) {
				char cmin = pivot_trans(cand[cidx[next]], curp);
				for (size_t j = jb + 1U; j < je; j++) {
					if (pivot_trans(cand[cidx[j]], curp) < cmin) {
						next = j;
						cmin = pivot_trans(cand[cidx[next]], curp);
					}
				}
				curp = cand[cidx[next]];
				npiv = (char)(!w ? curp : npiv);
				if (UNLIKELY(npiv <= prev)) {
					/* avoid loops */
					goto out;
				}
				fputc(curp, stdout);
				cidx[next]++;
			}
			for (;;) {
				unsigned int min = jb;
				char cmin = pivot_trans(cand[cidx[min]], curp);

				/* find the smallest item that is >= curp */
				for (size_t j = jb + 1U; j < je; j++) {
					if (pivot_trans(cand[cidx[j]], curp) < cmin) {
						min = j;
						cmin = pivot_trans(cand[cidx[min]], curp);
					}
				}
				if (UNLIKELY(cand[cidx[min]] == '~')) {
					break;
				}
				curp = cand[cidx[min]];
				fputc(curp, stdout);
				cidx[min]++;
			}
		}
		fputc('\n', stdout);
	}
out:
	free(cidx);
	return;
}

void
long_actcon(const struct actcon_s *spec, char from, char till)
{
/* print the currently active contract with the longest history
 * assuming that the expiration of one contract spawns the next one */
	size_t ys[32U];

	memset(ys, 0, sizeof(ys));
	for (size_t j = 0U; j < spec->nsum; j++) {
		size_t y = ((spec->sum[j].m >> 1U) - 1U) / spec->sum[j].l + 1U;
		y *= spec->sum[j].n;
		/* go through contracts and assign the number of years
		 * of history per each contract month */
		for (size_t i = 0U; i < spec->sum[j].l; i++) {
			char xp = spec->sum[j].x[i];
			ys[xp - '@'] += y;
		}
	}
	from = (char)(from >= '@' ? from : '@');
	till = (char)(till <= 'Z' ? till : 'Z');

	for (char npiv = from; npiv <= till; npiv++) {
		size_t max = 0U;
		char cand;

		/* fast forward npiv to actual contracts */
		for (; npiv <= till && !ys[npiv - '@']; npiv++);

		/* now start at pivot and find the first maximum */
		for (char cc = npiv; cc <= 'Z'; cc++) {
			if (ys[cc - '@'] <= max) {
				continue;
			}
			cand = cc;
			max = ys[(cand = cc) - '@'];
		}
		/* ... flip around if need be */
		for (char cc = '@'; cc < npiv; cc++) {
			if (ys[cc - '@'] <= max) {
				continue;
			}
			cand = cc;
			max = ys[(cand = cc) - '@'];
		}

		fputc(cand, stdout);
		fputc('\n', stdout);
	}
	return;
}

/* actcon.c ends here */
