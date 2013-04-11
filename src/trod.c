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
#include <sys/mman.h>
#if defined WORDS_BIGENDIAN
# include <limits.h>
#endif	/* WORDS_BIGENDIAN */

#include "truffle.h"
#include "dt-strpf.h"
#include "trod.h"
#include "gq.h"

#if defined STANDALONE
# include <stdio.h>
#endif	/* STANDALONE */
#if !defined __GNUC__ && !defined __INTEL_COMPILER
# define __builtin_expect(x, y)	x
#endif	/* !GCC && !ICC */
#if defined __INTEL_COMPILER
# pragma warning (disable:1572)
#endif	/* __INTEL_COMPILER */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */
#if defined DEBUG_FLAG
# include <assert.h>
# define TRUF_DEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define assert(args...)
# define TRUF_DEBUG(args...)
#endif	/* DEBUG_FLAG */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*(x)))
#endif	/* !countof */

#if !defined PROT_MEM
# define PROT_MEM		(PROT_READ | PROT_WRITE)
# define MAP_MEM		(MAP_PRIVATE | MAP_ANONYMOUS)
#endif	/* !PROT_MEM */

typedef struct trod_state_s *trod_state_t;
typedef struct trod_event_s *trod_event_t;

struct trod_state_s {
	uint8_t val;
	uint8_t month;
	uint16_t year;
};

/* a single trod event,
 * this coincides with echse's struct echs_event_s only that the state is
 * not a string but already dissected into value, month and year;
 * also we're working in state mode so WHAT is a 0-state terminated list
 * of states at the instant WHEN */
struct trod_event_s {
	trod_instant_t when;
	struct trod_state_s what[];
};

/* trod container */
struct trod_s {
	size_t ninst;
	size_t nev;
	/* 0_event terminated list of events (NINST of them) */
	trod_event_t ev[];
};

/* trod event list, in terms of gq.h */
struct troqi_s {
	struct gq_item_s i;
	/* an exploded trod_event_s, we'll use ev.what[0] to access what */
	struct trod_event_s ev;
	struct trod_state_s what;
};

struct troq_s {
	size_t ninst;
	size_t nev;
	/* linked list of struct troq_s objects */
	struct gq_ll_s trev[1];
};


#if !defined STANDALONE
/* helpers */
static uint32_t
strtoui(const char *str, const char **ep)
{
	uint32_t res = 0U;
	const char *sp;

	for (sp = str; *sp >= '0' && *sp <= '9'; sp++) {
		res *= 10;
		res += *sp - '0';
	}
	*ep = (const char*)sp;
	return res;
}
#endif	/* !STANDALONE */

static void*
make_gq_item(gq_t x, size_t nmemb, size_t membz)
{
	if (x->free->i1st == GQ_NULL_ITEM) {
		/* resize */
		init_gq(x, nmemb, membz);
	}
	/* get them object */
	return gq_pop_head(x->free);
}

static void
free_gq_item(gq_t x, void *i)
{
	/* back on our free list */
	gq_push_tail(x->free, i);
	return;
}


/* troq and trod guts */
static struct trod_event_s nil_ev = {0};
#define NIL_EVENT	(&nil_ev)

/* shared between trod_{add,pop}_event() */
static struct gq_s pool[1];

static void
troq_add_event(struct troq_s tgt[static 1], trod_event_t ev)
{
	struct troqi_s *qi;
	trod_instant_t last;

	if (LIKELY((qi = (struct troqi_s*)tgt->trev->ilst) != NULL)) {
		last = qi->ev.when;
	} else {
		last = (trod_instant_t){0};
	}

	/* ctor a new troqi and populate */
	qi = make_gq_item(pool, 64U, sizeof(*qi));
	qi->ev = *ev;
	qi->what = ev->what[0];
	/* update counters */
	if (trod_inst_lt_p(last, ev->when)) {
		tgt->ninst++;
	}
	tgt->nev++;

	gq_push_tail(tgt->trev, (gq_item_t)qi);
	return;
}

static trod_event_t
troq_pop_event(struct troq_s src[static 1])
{
	struct troqi_s *qi;
	static struct {
		struct trod_event_s ev;
		struct trod_state_s st;
	} res;

	if (UNLIKELY((qi = (void*)gq_pop_head(src->trev)) == NULL)) {
		return NIL_EVENT;
	}

	res.ev = qi->ev;
	res.st = qi->what;
	free_gq_item(pool, (gq_item_t)qi);
	return &res.ev;
}

#if !defined STANDALONE
static trod_event_t
read_trod_event(const char *line, size_t UNUSED(llen))
{
/* just a normal echse edge line */
	static struct {
		struct trod_event_s ev;
		struct trod_state_s st;
	} res;
	const char *p;

	if ((p = strchr(line, '\t')) == NULL) {
		goto nul;
	} else if (trod_inst_0_p(res.ev.when = dt_strp(line))) {
		goto nul;
	}

	/* otherwise it's a match, snarf the val month year bit */
	switch (*++p) {
	case '~':
		res.st.val = 0U;
		p++;
		break;
	default:
		res.st.val = 1U;
		break;
	}

	/* now it's either YYYY-MM, or M-YYYY or M-dy where DY is relative
	 * to the year portion of I */
	switch (*p) {
	case 'f': case 'F':
		res.st.month = 1U;
		goto read_ui;
	case 'g': case 'G':
		res.st.month = 2U;
		goto read_ui;
	case 'h': case 'H':
		res.st.month = 3U;
		goto read_ui;
	case 'j': case 'J':
		res.st.month = 4U;
		goto read_ui;
	case 'k': case 'K':
		res.st.month = 5U;
		goto read_ui;
	case 'm': case 'M':
		res.st.month = 6U;
		goto read_ui;
	case 'n': case 'N':
		res.st.month = 7U;
		goto read_ui;
	case 'q': case 'Q':
		res.st.month = 8U;
		goto read_ui;
	case 'u': case 'U':
		res.st.month = 9;
		goto read_ui;
	case 'v': case 'V':
		res.st.month = 10U;
		goto read_ui;
	case 'x': case 'X':
		res.st.month = 11U;
		goto read_ui;
	case 'z': case 'Z':
		res.st.month = 12U;
		goto read_ui;
		/* fall through to int parsing */
	read_ui:
	case '0' ... '9': {
		const char *q;
		uint32_t ym = strtoui(p, &q);

		if (UNLIKELY(q == p)) {
			goto nul;
		} else if (ym < 1024U) {
			/* something like G0 or F4 or so */
			res.st.year = (uint16_t)(ym + res.ev.when.y);
		} else if (ym < 4096U) {
			/* absolute year, G2032 or H2102*/
			res.st.year = (uint16_t)ym;
			if (!res.st.month && *q == '-') {
				/* %Y-%m syntax? */
				p = q + 1;
				if (LIKELY((ym = strtoui(p, &q)) &&
					   q > p && ym <= 12U)) {
					res.st.month = (uint8_t)ym;
				}
			}
		} else if (ym < 299913U) {
			/* that's %Y%m syntax innit? */
			res.st.year = (uint16_t)(ym / 100U);
			res.st.month = (uint8_t)(ym % 100U);
		} else if (ym < 29991232U) {
			/* %Y%m%d syntax but we can't deal with d */
			res.st.year = (uint16_t)((ym / 100U) / 100U);
			res.st.month = (uint8_t)((ym / 100U) % 100U);
		}
		break;
	}
	default:
		goto nul;
	}
	return &res.ev;
nul:
	return NIL_EVENT;
}

static struct troq_s
read_troq(FILE *f)
{
	struct troq_s q = {0UL, 0UL};
	trod_instant_t last = {0};
	size_t llen = 0UL;
	char *line = NULL;
	ssize_t nrd;

	while ((nrd = getline(&line, &llen, f)) > 0) {
		trod_event_t ev = read_trod_event(line, nrd);

		if (trod_inst_le_p(last, ev->when)) {
			troq_add_event(&q, ev);
			last = ev->when;
		}
	}

	if (line) {
		free(line);
	}
	return q;
}
#endif	/* !STANDALONE */

static inline trod_event_t
chunk_inc_when(trod_event_t cp)
{
	return (void*)((char*)cp + sizeof(*cp));
}

static inline trod_event_t
chunk_inc_what(trod_event_t cp)
{
	return (void*)((char*)cp + sizeof(*cp->what));
}

static trod_t
troq_to_trod(struct troq_s q)
{
/* go over the queue Q and arrange stuff in arrays and array-of-arrays */
	trod_instant_t last = {0};
	trod_event_t chunk;
	trod_event_t cp;
	size_t chunz;
	size_t widx;
	trod_t res;

	/* first up, the number of (distinct) instants */
	res = malloc(sizeof(*res) + q.ninst * sizeof(*res->ev));
	res->ninst = 0UL;
	res->nev = 0UL;

	/* we also know about the total number of events */
	chunz = q.ninst * sizeof(*chunk) +
		(q.nev + q.ninst/*for 0-event*/) * sizeof(*chunk->what);
	chunk = malloc(chunz);
	memset(chunk, 0, chunz);

	/* populate RES and CHUNK now */
	cp = (void*)((char*)chunk - sizeof(*chunk->what));

	for (trod_event_t ev, c;
	     (ev = troq_pop_event(&q), !trod_inst_0_p(ev->when));) {
		if (trod_inst_lt_p(last, ev->when)) {
			/* start a new chamber */
			size_t iidx = res->ninst++;
			widx = 0UL;

			cp = chunk_inc_what(cp);
			res->ev[iidx] = c = cp;
			c->when = last = ev->when;
			cp = chunk_inc_when(cp);
		}

		/* aggregate */
		c->what[widx++] = *ev->what;
		cp = chunk_inc_what(cp);
		res->nev++;
	}


	/* some invariants */
	cp = chunk_inc_what(cp);
	assert(res->ninst == q.ninst);
	assert(res->nev == q.nev);
	assert(chunz == (char*)cp - (char*)chunk);
	return res;
}


/* public API */
#if !defined STANDALONE
DEFUN void
free_trod(trod_t td)
{
	free(td);
	return;
}

DEFUN trod_t
read_trod(const char *file)
{
/* lines look like
 * DATETIME \t [~] MONTH YEAR ... */
	struct troq_s q;
	FILE *f;
	trod_t res;

	if (file[0] == '-' && file[1] == '\0') {
		f = stdin;
	} else if ((f = fopen(file, "r")) == NULL) {
		fprintf(stderr, "unable to open file %s\n", file);
		return NULL;
	}

	/* temporary troq queue */
	q = read_troq(f);
	fclose(f);

	res = troq_to_trod(q);
	return res;
}
#endif	/* !STANDALONE */


/* converter, schema->trod */
#if defined STANDALONE
static int opt_oco;
static int opt_abs;

struct cnode_s {
	idate_t x;
	daysi_t l;
	double y __attribute__((aligned(sizeof(double))));
};

struct cline_s {
	daysi_t valid_from;
	daysi_t valid_till;
	char month;
	int8_t year_off;
	size_t nn;
	struct cnode_s n[];
};

struct trsch_s {
	size_t np;
	struct cline_s *p[];
};

static char
i_to_m(uint8_t month)
{
	static char months[] = "?FGHJKMNQUVXZ";
	return months[month];
}

static __attribute__((unused)) uint8_t
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
daysi_to_year(daysi_t dd)
{
	unsigned int y;
	unsigned int j00;

	/* get year first (estimate) */
	y = dd / 365U;
	/* get jan-00 of (est.) Y */
	j00 = y * 365U + y / 4U;
	/* y correct? */
	if (UNLIKELY(j00 >= dd)) {
		/* correct y */
		y--;
	}
	/* ass */
	return TO_YEAR(y);
}

#define DAYSI_DIY_BIT	(1U << (sizeof(daysi_t) * 8 - 1))

static daysi_t
daysi_in_year(daysi_t ds, unsigned int y)
{
	unsigned int j00;
	unsigned int by = TO_BASE(y);

	if (UNLIKELY(!(ds & DAYSI_DIY_BIT))) {
		/* we could technically do something here */
		return ds;
	}

	ds &= ~DAYSI_DIY_BIT;

	/* get jan-00 of (est.) Y */
	j00 = by * 365U + by / 4U;

	if (UNLIKELY(y % 4U == 0) && ds >= 60U) {
		ds++;
	}
	return ds + j00;
}

static void
troq_add_clines(struct troq_s q[static 1], trsch_t sch, daysi_t when)
{
	static struct {
		struct trod_event_s ev;
		struct trod_state_s st;
	} qi;
	unsigned int y = daysi_to_year(when);

	qi.ev.when = daysi_to_trod_instant(when);
	for (size_t i = 0; i < sch->np; i++) {
		struct cline_s *p = sch->p[i];

		/* check year validity */
		if (when < p->valid_from || when > p->valid_till) {
			/* cline isn't applicable */
			continue;
		}
		for (size_t j = 0; j < p->nn - 1; j++) {
			struct cnode_s *n1 = p->n + j;
			struct cnode_s *n2 = n1 + 1;
			daysi_t l1 = daysi_in_year(n1->l, y);
			daysi_t l2 = daysi_in_year(n2->l, y);

			if (when >= l1 && when <= l2) {
				/* something happened between l1 and l2 */
				if (when == l2 && n2->y == 0.0) {
					qi.st.val = 0U;
				} else if (when == l1 && n1->y != 0.0) {
					qi.st.val = 1U;
				} else if (when == l1 + 1U && n1->y == 0.0) {
					qi.st.val = 1U;
				} else {
					continue;
				}
				qi.st.month = m_to_i(p->month);
				qi.st.year = (uint16_t)(y + p->year_off);

				/* just add the guy */
				troq_add_event(q, &qi.ev);
				break;
			}
		}
	}
	return;
}

static trod_t
schema_to_trod(trsch_t sch, idate_t from, idate_t till)
{
	struct troq_s q = {0UL, 0UL};
	daysi_t fsi = idate_to_daysi(from);
	daysi_t tsi = idate_to_daysi(till);
	trod_t res;

	for (daysi_t now = fsi; now < tsi; now++) {
		troq_add_clines(&q, sch, now);
	}

	res = troq_to_trod(q);
	return res;
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
	for (const struct trod_state_s *s = ev->what; s->month; s++, p = var) {
		if (!s->val) {
			*p++ = '~';
		}

		if (!opt_oco) {
			unsigned int y = s->year;

			if (!opt_abs && ev->when.y <= y) {
				y -= ev->when.y;
			}
			p += snprintf(
				p, sizeof(buf) - (p - buf),
				"%c%u", i_to_m(s->month), y);
		} else {
			p += snprintf(
				p, sizeof(buf) - (p - buf),
				"%hu%02u", s->year, (unsigned int)s->month);
		}

		*p++ = '\n';
		*p = '\0';
		fputs(buf, whither);
	}
	return;
}

static void
print_trod(trod_t td, FILE *whither)
{
	for (size_t i = 0; i < td->ninst; i++) {
		trod_event_t x = td->ev[i];

		print_trod_event(x, whither);
	}
	return;
}
#endif	/* STANDALONE */


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "trod-clo.h"
#include "trod-clo.c"
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
	if (argi->from_given) {
		from = read_date(argi->from_arg, NULL);
	} else {
		from = 20000101U;
	}
	if (argi->till_given) {
		till = read_date(argi->till_arg, NULL);
	} else {
		till = 20371231U;
	}
	if (argi->oco_given) {
		opt_oco = 1;
		opt_abs = 1;
	} else if (argi->abs_given) {
		opt_abs = 1;
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
