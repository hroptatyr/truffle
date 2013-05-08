/*** schema.c -- read and write roll-over descriptions aka schemas
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
#include <string.h>
#include <ctype.h>

#include "truffle.h"
#include "schema.h"
#include "cut.h"
#include "yd.h"
#include "dt-strpf.h"
#include "mmy.h"

#include "daisy.c"

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

typedef struct cline_s *cline_t;

/* a node */
struct cnode_s {
	idate_t x;
	daysi_t l;
	double y __attribute__((aligned(sizeof(double))));
};

/* a single line */
#define DFLT_FROM	(101)
#define DFLT_TILL	(1048576)
struct cline_s {
	daysi_t valid_from;
	daysi_t valid_till;
	char month;
	int8_t year_off;
	size_t nn;
	struct cnode_s n[];
};

/* schema */
struct trsch_s {
	size_t np;
	struct cline_s *p[];
};



/* standalone version, we could use ds_sum but this is most likely
 * causing more cache misses */
static idate_t
daysi_to_idate(daysi_t dd)
{
/* given days since 2000-01-00 (Mon),
 * compute the idate_t representation X so that idate_to_daysi(X) == DDT */
/* stolen from dateutils' daisy.c */
	int y;
	int m;
	int d;
	int j00;
	unsigned int doy;

	/* get year first (estimate) */
	y = dd / 365U;
	/* get jan-00 of (est.) Y */
	j00 = y * 365U + y / 4U;
	/* y correct? */
	if (UNLIKELY(j00 >= (int)dd)) {
		/* correct y */
		y--;
		/* and also recompute the j00 of y */
		j00 = y * 365U + y / 4U;
	}
	/* ass */
	y = TO_YEAR(y);
	/* this one must be positive now */
	doy = dd - j00;

	/* get month and day from doy */
	{
		struct md_s md = __yd_to_md((struct yd_s){y, doy});
		m = md.m;
		d = md.d;
	}
	return (y * 100U + m) * 100U + d;
}


static void
__err_not_asc(const char *line, size_t llen)
{
	fputs("error: ", stderr);
	fwrite(line, llen, 1, stderr);
	fputs("error: dates are not in ascending order\n", stderr);
	return;
}

static cline_t
make_cline(char month, int yoff)
{
	cline_t res;
	unsigned int mo;

	if (!(mo = m_to_i(month))) {
		return NULL;
	}
	/* otherwise alloc and populate RES */
	res = malloc(sizeof(*res));
	res->month = i_to_m(mo);
	res->year_off = (int8_t)yoff;
	res->nn = 0;
	return res;
}

static cline_t
cline_add_sugar(cline_t cl, idate_t x, double y)
{
#define CN_STEP		(4)
	size_t idx;

	if ((cl->nn % CN_STEP) == 0) {
		size_t new = sizeof(*cl) + (cl->nn + CN_STEP) * sizeof(*cl->n);
		cl = realloc(cl, new);
	}
	idx = cl->nn++;
	cl->n[idx].x = x;
	cl->n[idx].y = y;
	cl->n[idx].l = daysi_sans_year(x);
	return cl;
}

static trsch_t
sch_add_cl(trsch_t s, struct cline_s *cl)
{
#define CL_STEP		(16)
	if (s == NULL) {
		size_t new = sizeof(*s) + CL_STEP * sizeof(*s->p);
		s = malloc(new);
		s->np = 0;
	} else if ((s->np % CL_STEP) == 0) {
		size_t new = sizeof(*s) + (s->np + CL_STEP) * sizeof(*s->p);
		s = realloc(s, new);
		memset(s->p + s->np, 0, CL_STEP * sizeof(*s->p));
	}
	s->p[s->np++] = cl;
	return s;
}

static cline_t
__read_schema_line(const char *line, size_t llen)
{
	cline_t cl = NULL;
	static const char skip[] = " \t";

	switch (line[0]) {
		int yoff;
		const char *p;
		char *tmp;
		idate_t dt;
		double v;
	case 'f': case 'F':
	case 'g': case 'G':
	case 'h': case 'H':
	case 'j': case 'J':
	case 'k': case 'K':
	case 'm': case 'M':
	case 'n': case 'N':
	case 'q': case 'Q':
	case 'u': case 'U':
	case 'v': case 'V':
	case 'x': case 'X':
	case 'z': case 'Z':
		if (!isspace(line[1])) {
			yoff = strtol(line + 1, &tmp, 10);
			p = tmp + strspn(tmp, skip);
		} else {
			p = line + strspn(line + 1, skip) + 1;
			yoff = 0;
		}

		/* kick off the schema-line process */
		if ((cl = make_cline(line[0], yoff)) == NULL) {
			break;
		}

		do {
			daysi_t ddt;

			if (!(dt = read_date(p, &tmp) % 10000U)) {
				goto nope;
			}
			p = tmp + strspn(tmp, skip);
			v = strtod(p, &tmp);
			p = tmp + strspn(tmp, skip);
			if (UNLIKELY(cl->nn == 0)) {
				/* auto-fill to the left */
				if (UNLIKELY(v != 0.0 && dt != 101)) {
					cl = cline_add_sugar(cl, 101, v);
				}
			}
			/* add this line */
			ddt = daysi_sans_year(dt);
			if (cl->nn && ddt <= cl->n[cl->nn - 1].l) {
				__err_not_asc(line, llen);
				goto nope;
			}
			cl = cline_add_sugar(cl, dt, v);
		} while (*p != '\n');
		/* auto-fill 1 polygons to the right */
		if (UNLIKELY(v != 0.0 && dt != 1231)) {
			cl = cline_add_sugar(cl, 1231, v);
		}
	default:
		break;
	}
	return cl;
nope:
	free(cl);
	return NULL;
}

static cline_t
read_schema_line(const char *line, size_t llen)
{
/* schema lines can be prefixed with a range of validity dates:
 * * valid for all years,
 * * - 2002-99-99 valid for all years up to and including 2002
 * 2003-00-00 - * valid from 2003
 * 2002-00-00 - 2003-99-99 valid in 2002 and 2003
 * * - 1989-07-22 valid until (incl.) 22 Jul 1989 */
	static const char skip[] = " \t";
	cline_t cl;
	/* validity */
	daysi_t vfrom = 0;
	daysi_t vtill = 0;
	const char *lp = line;

	while (1) {
		switch (*lp) {
			char *tmp;
			idate_t tmi;
		case '0' ... '9':
			tmi = read_date(lp, &tmp);

			if (!vfrom) {
				vfrom = idate_to_daysi(tmi);
			} else {
				vtill = idate_to_daysi(tmi);
			}
			lp = tmp + strspn(tmp, skip);
			continue;
		case '*':
			vfrom = vfrom ?: DFLT_FROM;
			vtill = DFLT_TILL;
			lp += strspn(lp + 1, skip) + 1;
			continue;
		case '-':
			lp += strspn(lp + 1, skip) + 1;
			vtill = DFLT_TILL;
			continue;
		default:
			break;
		}
		break;
	}
	if (vtill > DFLT_FROM && vfrom > vtill) {
		return NULL;
	} else if (vtill == 0 && vfrom == 0) {
		vfrom = DFLT_FROM;
		vtill = DFLT_TILL;
	}
	if ((cl = __read_schema_line(lp, llen - (lp - line)))) {
		cl->valid_from = vfrom;
		cl->valid_till = vtill ?: vfrom;
	}
	return cl;
}

static void
print_cline(cline_t cl, FILE *whither)
{
	if (cl->valid_from == DFLT_FROM && cl->valid_till == DFLT_TILL) {
		/* print nothing */
	} else {
		if (cl->valid_from == DFLT_FROM) {
			fputc('*', whither);
		} else if (cl->valid_from > DFLT_FROM) {
			fprintf(whither, "%u", daysi_to_idate(cl->valid_from));
		} else {
			/* we were meant to fill this bugger */
			abort();
		}
		if (cl->valid_from < cl->valid_till) {
			fputc('-', whither);
			if (cl->valid_till >= DFLT_TILL) {
				fputc('*', whither);
			} else if (cl->valid_from > 0) {
				fprintf(whither, "%u",
					daysi_to_idate(cl->valid_till));
			} else {
				/* invalid value in here */
				abort();
			}
		}
		fputc(' ', whither);
	}

	fputc(cl->month, whither);
	if (cl->year_off) {
		fprintf(whither, "%d", cl->year_off);
	}
	for (size_t i = 0; i < cl->nn; i++) {
		idate_t dt = cl->n[i].x;
		fputc(' ', whither);
		fprintf(whither, " %04u %.8g", dt, cl->n[i].y);
	}
	fputc('\n', whither);
	return;
}


/* public API */
DEFUN trsch_t
read_schema(const char *file)
{
/* lines look like
 * MONTH YEAR_OFF SPACE DATE VAL ... */
	size_t llen = 0UL;
	char *line = NULL;
	trsch_t res = NULL;
	FILE *f;
	ssize_t nrd;

	if (file[0] == '-' && file[1] == '\0') {
		f = stdin;
	} else if ((f = fopen(file, "r")) == NULL) {
		fprintf(stderr, "unable to open file %s\n", file);
		return NULL;
	}
	while ((nrd = getline(&line, &llen, f)) > 0) {
		cline_t cl;

		if ((cl = read_schema_line(line, nrd))) {
			res = sch_add_cl(res, cl);
		}
	}

	if (line) {
		free(line);
	}
	fclose(f);
	return res;
}

DEFUN void
print_schema(trsch_t sch, FILE *whither)
{
	for (size_t i = 0; i < sch->np; i++) {
		print_cline(sch->p[i], whither);
	}
	return;
}

DEFUN void
free_schema(trsch_t sch)
{
	for (size_t i = 0; i < sch->np; i++) {
		free(sch->p[i]);
	}
	free(sch);
	return;
}


/* cuts, this is the glue between schema and cut */
DEFUN trcut_t
make_cut(trcut_t old, trsch_t sch, daysi_t when)
{
	trcut_t res = old;
	int y = daysi_to_year(when);

	if (old) {
		/* quickly rinse the old cut */
		for (size_t i = 0; i < old->ncomps; i++) {
			old->comps[i].y = 0.0;
		}
	}
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
				/* something happened between n1 and n2 */
				struct trcc_s cc;
				double xsub = l2 - l1;
				double tsub = when - l1;
				double ysub = n2->y - n1->y;

				cc.month = p->month;
				cc.year = (uint16_t)(y + p->year_off);
				cc.y = n1->y + tsub * ysub / xsub;

				/* try and find that guy in the old cut */
				res = cut_add_cc(res, cc);
				break;
			}
		}
	}
	return res;
}

DEFUN void
free_cut(trcut_t cut)
{
	free(cut);
	return;
}

/* schema.c ends here */
