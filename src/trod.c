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
#include <ctype.h>
#include <math.h>
#include <assert.h>

#include "truffle.h"
#include "instant.h"
#include "daisy.h"
#include "trod.h"
#include "mmy.h"
#include "truf-dfp754.h"
#include "nifty.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:1572)
#endif /* __INTEL_COMPILER */

#if !defined MAP_ANON && defined MAP_ANONYMOUS
# define MAP_ANON	MAP_ANONYMOUS
#elif defined MAP_ANON
/* all's good */
#else  /* !MAP_ANON && !MAP_ANONYMOUS */
# define MAP_ANON	(0U)
#endif	/* !MAP_ANON && MAP_ANONYMOUS */

#if !defined MAP_MEM
# define MAP_MEM	(MAP_ANON | MAP_PRIVATE)
#endif	/* !MAP_MEM */
#if !defined PROT_MEM
# define PROT_MEM	(PROT_READ | PROT_WRITE)
#endif	/* !PROT_MEM */

#define __static_assert(COND, MSG)				\
	typedef char static_assertion_##MSG[2 * (!!(COND)) - 1]
#define __static_assert3(X, L)	__static_assert(X, static_assert_##L)
#define __static_assert2(X, L)	__static_assert3(X, L)
#define static_assert(X)	__static_assert2(X, __LINE__)


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


/* converter, schema->trod */
#if defined STANDALONE
static int opt_oco;
static int opt_abs;

struct cnode_s {
	idate_t x;
	daisy_t l;
	double y __attribute__((aligned(sizeof(double))));
};

struct cline_s {
	daisy_t valid_from;
	daisy_t valid_till;
	char month;
	int8_t year_off;
	size_t nn;
	struct cnode_s n[];
};

struct trsch_s {
	size_t np;
	struct cline_s *p[];
};

/* auxiliaries to disappear */
static inline char
i_to_m(unsigned int month)
{
	static char months[] = "?FGHJKMNQUVXZ";
	return months[month];
}

static inline unsigned int
m_to_i(char month)
{
	switch (month) {
	case 'f': case 'F':
		return 1U;
	case 'g': case 'G':
		return 2U;
	case 'h': case 'H':
		return 3U;
	case 'j': case 'J':
		return 4U;
	case 'k': case 'K':
		return 5U;
	case 'm': case 'M':
		return 6U;
	case 'n': case 'N':
		return 7U;
	case 'q': case 'Q':
		return 8U;
	case 'u': case 'U':
		return 9;
	case 'v': case 'V':
		return 10U;
	case 'x': case 'X':
		return 11U;
	case 'z': case 'Z':
		return 12U;
	default:
		break;
	}
	return 0U;
}

static int
troq_add_cline(trod_event_t qi, const struct cline_s *p, daisy_t when)
{
	unsigned int y = daisy_to_year(when);

	for (size_t j = 0; j < p->nn - 1; j++) {
		const struct cnode_s *n1 = p->n + j;
		const struct cnode_s *n2 = n1 + 1;
		daisy_t l1 = daisy_in_year(n1->l, y);
		daisy_t l2 = daisy_in_year(n2->l, y);

		if (when == l2) {
			/* something happened at l2 */
			if (n2->y == 0.0 && n1->y != 0.0) {
				qi->what->exp = 0.df;
			} else if (n2->y != 0.0 && n1->y == 0.0) {
				qi->what->exp = 1.df;
			} else {
				continue;
			}
		} else if (j == 0 && when == l1) {
			/* something happened at l1 */
			if (UNLIKELY(n1->y != 0.0)) {
				qi->what->exp = 2.df;
			} else {
				continue;
			}
		} else {
			continue;
		}
		qi->what->sym =
			make_truf_mmy(y + p->year_off, m_to_i(p->month), 0U);

		/* indicate success (as in clear for adding) */
		return 0;
	}
	/* indicate failure (to add anything) */
	return -1;
}

static void
troq_add_clines(struct troq_s q[static 1], trsch_t sch, daisy_t when)
{
	static struct {
		struct trod_event_s ev;
		truf_trod_t st;
	} qi;

	qi.ev.when = daisy_to_instant(when);
	for (size_t i = 0; i < sch->np; i++) {
		const struct cline_s *p = sch->p[i];

		/* check year validity */
		if (when < p->valid_from || when > p->valid_till) {
			/* cline isn't applicable */
			;
		} else if (troq_add_cline(&qi.ev, p, when) < 0) {
			/* nothing added then */
			;
		} else {
			/* just add the guy */
			troq_add_event(q, &qi.ev);
		}
	}
	return;
}

static trod_t
schema_to_trod(trsch_t sch, idate_t from, idate_t till)
{
	struct troq_s q = {0UL, 0UL};
	daisy_t fsi = idate_to_daisy(from);
	daisy_t tsi = idate_to_daisy(till);
	trod_t res;

	for (daisy_t now = fsi; now < tsi; now++) {
		troq_add_clines(&q, sch, now);
	}

	res = troq_to_trod(q);
	return res;
}


#undef DEFUN
#undef DECLF
#define DECLF		static
#define DEFUN		static __attribute__((unused))
#include "gbs.h"
#include "gbs.c"

/* bitset for active contracts */
static struct gbs_s active[1U];

static void
activate(int ry, unsigned int m)
{
	gbs_set(active, 12 * ry + (m - 1));
	return;
}

static void
deactivate(int ry, unsigned int m)
{
	gbs_unset(active, 12 * ry + (m - 1));
	return;
}

static int
activep(int ry, unsigned int m)
{
	return gbs_set_p(active, 12 * ry + (m - 1));
}

static void
flip_over(int ry)
{
/* flip over to a new year in the ACTIVE bitset */
	gbs_shift_lsb(active, 12 * ry);
	return;
}


static void
print_trod_event(trod_event_t ev, FILE *whither)
{
	char buf[64];
	char *p = buf;
	char *var;

	p += dt_strf(buf, sizeof(buf), ev->when);
	*p++ = '\t';
	var = p;
	for (const struct truf_trod_s *s = ev->what; s->sym; s++, p = var) {
		unsigned int m = truf_mmy_mon(s->sym);
		unsigned int y = truf_mmy_year(s->sym);
		int ry = y - ev->when.y;

		if (s->exp == 0.df) {
			*p++ = '~';
			deactivate(ry, m);
		} else if (s->exp > 1.df && activep(ry, m)) {
			continue;
		} else {
			activate(ry, m);
		}

		if (!opt_oco) {
			if (!opt_abs && ev->when.y <= y) {
				y -= ev->when.y;
			}
			p += snprintf(
				p, sizeof(buf) - (p - buf),
				"%c%u", i_to_m(m), y);
		} else {
			p += snprintf(
				p, sizeof(buf) - (p - buf),
				"%u%02u", y, m);
		}

		*p++ = '\n';
		*p = '\0';
		fputs(buf, whither);
	}
	return;
}

static void
print_flip_over(trod_event_t ev, FILE *whither)
{
	static unsigned int last_y;
	char buf[64];
	char *p = buf;
	char *var;
	unsigned int y = ev->when.y;
	unsigned int ry;

	if (UNLIKELY(last_y == 0 || last_y > y)) {
		last_y = y;
		return;
	} else if (LIKELY((ry = (y - last_y)) == 0U)) {
		return;
	} else if (opt_abs || opt_oco) {
		goto flip_over;
	}

	/* otherwise it's a flip-over, print all active (in the old year) */
	p += dt_strf(buf, sizeof(buf), (echs_instant_t){y, 1, 1, ECHS_ALL_DAY});
	*p++ = '\t';
	var = p;

	for (size_t i = 0; i < active->nbits; i++, p = var) {
		unsigned int yr = i / 12U;
		unsigned int mo = i % 12U;

		if (activep(yr, mo + 1U)) {
			char cmo = i_to_m(mo + 1U);

			p += snprintf(
				p, sizeof(buf) - (p - buf),
				"%c%u->%c%d",
				cmo, yr, cmo, (int)yr - (int)ry);

			*p++ = '\n';
			*p = '\0';
			fputs(buf, whither);
		}
	}
flip_over:
	/* now do the flip-over and reprint */
	flip_over(ry);
	last_y = y;
	return;
}

static void
print_trod(trod_t td, FILE *whither)
{
	/* initialise the flip-over book-keeper */
	init_gbs(active, 12U * 30U);

	for (size_t i = 0; i < td->ninst; i++) {
		trod_event_t x = td->ev[i];

		print_flip_over(x, whither);
		print_trod_event(x, whither);
	}

	fini_gbs(active);
	return;
}
#endif	/* STANDALONE */


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "trod.xh"
#include "trod.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct tr_args_info argi[1];
	trsch_t sch = NULL;
	trod_t td = NULL;
	idate_t from;
	idate_t till;
	int res = 0;

	if (tr_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	if (argi->from_given) {
		from = read_idate(argi->from_arg, NULL);
	} else {
		from = 20000101U;
	}
	if (argi->till_given) {
		till = read_idate(argi->till_arg, NULL);
	} else {
		till = 20371231U;
	}
	if (argi->oco_given) {
		opt_oco = 1;
		opt_abs = 1;
	} else if (argi->abs_given) {
		opt_abs = 1;
	}
	if (argi->inputs_num > 0) {
		sch = read_schema(argi->inputs[0]);
	} else {
		sch = read_schema("-");
	}
	if (UNLIKELY(sch == NULL)) {
		/* try the trod reader */
		if (argi->inputs_num == 0 ||
		    ((td = read_trod(argi->inputs[0])) == NULL)) {
			fputs("schema unreadable\n", stderr);
			res = 1;
			goto out;
		} else if (td != NULL) {
			/* we're trod already, do fuckall */
			goto pr;
		}
	}

	/* convert that schema goodness */
	td = schema_to_trod(sch, from, till);

	/* schema not needed anymore */
	free_schema(sch);

	if (LIKELY(td != NULL)) {
	pr:
		/* and print it again */
		print_trod(td, stdout);

		/* and free the rest of our resources */
		free_trod(td);
	}

out:
	tr_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* trod.c ends here */
