#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#if defined STANDALONE
# include <stdio.h>
# define DECLF	static
# define DEFUN	static
#endif	/* STANDALONE */
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
# define TRUF_DEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define TRUF_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef uint32_t idate_t;
typedef struct trcut_s *trcut_t;
typedef struct trsch_s *trsch_t;
typedef struct cline_s *cline_t;

/* a node */
struct cnode_s {
	idate_t x;
	double y;
};

/* a single line */
#define DFLT_FROM	(1)
#define DFLT_TILL	(9999)
struct cline_s {
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

/* result structure, for cuts etc. */
struct trcc_s {
	char month;
	int8_t year_off;
	double y __attribute__((aligned(16)));
};

struct trcut_s {
	uint16_t year_off;

	size_t ncomps;
	struct trcc_s comps[];
};

DECLF trcut_t make_cut(trsch_t schema, idate_t when);
DECLF void free_cut(trcut_t);

DECLF trsch_t read_schema(const char *file);
DECLF void free_schema(trsch_t);


/* idate helpers */
static int8_t __attribute__((unused)) ml[] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};
static int16_t mc[] = {
/* this is 100 - ml[i] cumulated */
	0, 0, 69, 141, 210, 280, 349, 419, 488, 557, 627, 696, 766
};

static int
idate_sub(idate_t d1, idate_t d2)
{
	int m1, m2;
	int y1, y2;
	int res;

	if (d1 == d2) {
		return 0;
	}
	y1 = d1 / 10000;
	y2 = d2 / 10000;

	/* 0205 - 0128 -> 77 - (100 - 31) = 8
	 * 0305 - 0128 -> 177 - (100 - 31) - (100 - 28) = 36
	 * 0105 - 1228 -> -1123 - (69 - 835) = 8 */
	m1 = ((d1 / 100) % 100);
	m2 = ((d2 / 100) % 100);
	res = ((d2 - d1) - (mc[m2] - mc[m1]));
	if (y1 == y2) {
		return res % 365;
	}
	/* .... plus leap days actually */
	return (y2 - y1) * 365 + res;
}

static trsch_t
sch_add_cl(trsch_t s, struct cline_s *cl)
{
	if (s == NULL) {
		size_t new = sizeof(*s) + 16 * sizeof(*s->p);
		s = malloc(new);
	} else if ((s->np % 16) == 0) {
		size_t new = sizeof(*s) + (s->np + 16) * sizeof(*s->p);
		s = realloc(s, new);
	}
	s->p[s->np++] = cl;
	return s;
}

static trcut_t
cut_add_cc(trcut_t c, struct trcc_s cc)
{
	if (c == NULL) {
		size_t new = sizeof(*c) + 16 * sizeof(*c->comps);
		c = calloc(new, 1);
	} else if ((c->ncomps % 16) == 0) {
		size_t new = sizeof(*c) + (c->ncomps + 16) * sizeof(*c->comps);
		c = realloc(c, new);
		memset(c->comps + c->ncomps, 0, 16 * sizeof(*c->comps));
	}
	c->comps[c->ncomps++] = cc;
	return c;
}

static cline_t
make_cline(char month, int8_t yoff, size_t nnodes)
{
	cline_t res = malloc(sizeof(*res) + nnodes * (sizeof(*res->n)));

	res->month = month;
	res->year_off = yoff;
	res->nn = nnodes;
	return res;
}

static cline_t
cline_add_sugar(cline_t cl, idate_t x, double y)
{
	size_t idx;

	if ((cl->nn % 4) == 0) {
		size_t new = sizeof(*cl) + (cl->nn + 4) * sizeof(*cl->n);
		cl = realloc(cl, new);
	}
	idx = cl->nn++;
	cl->n[idx].x = x;
	cl->n[idx].y = y;
	return cl;
}

static idate_t
read_date(const char *str, char **ptr)
{
	idate_t res;
	char *tmp;

	if ((res = strtol(str, &tmp, 10)) &&
	    (*tmp == '\0' || isspace(*tmp))) {
		/* ah brilliant */
		if (ptr) {
			*ptr = tmp;
		}
		return res;
	}
	if (*tmp != '-') {
		/* porn date */
		if (ptr) {
			*ptr = tmp;
		}
		return 0;
	}
	str = tmp + 1;
	res *= 100;
	res += strtol(str, &tmp, 10);
	if (*tmp != '-') {
		/* porn date */
		if (ptr) {
			*ptr = tmp;
		}
		return res;
	}
	str = tmp + 1;
	res *= 100;
	res += strtol(str, &tmp, 10);
	if (*tmp != '\0') {
		/* porn date */
		if (ptr) {
			*ptr = tmp;
		}
		return 0;
	}
	if (ptr) {
		*ptr = tmp;
	}
	return res;
}

static size_t
snprint_idate(char *restrict buf, size_t bsz, idate_t dt)
{
	return snprintf(buf, bsz, "%u-%02u-%02u",
			dt / 10000, (dt % 10000) / 100, (dt % 100));
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

static cline_t
read_schema_line(const char *line, size_t llen __attribute__((unused)))
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
		cl = make_cline(line[0], yoff, 0);

		do {
			dt = read_date(p, &tmp) % 10000;
			p = tmp + strspn(tmp, skip);
			v = strtod(p, &tmp);
			p = tmp + strspn(tmp, skip);
			/* add this line */
			cl = cline_add_sugar(cl, dt, v);
		} while (*p != '\n');
	default:
		break;
	}
	return cl;
}

static cline_t
read_schema_line(const char *line, size_t UNUSED(llen))
{
/* schema lines can be prefixed with a range of validity years:
 * * valid for all years,
 * * - 2002 or * 2002 valid for all years up to and including 2002
 * 2003 - * valid from 2003
 * 2002 - 2003 or 2002 2003 valid in 2002 and 2003 */
	static const char skip[] = " \t";
	cline_t cl;
	/* validity */
	uint16_t vfrom = 0;
	uint16_t vtill = 0;
	const char *lp = line;

	while (1) {
		switch (*lp) {
			char *tmp;
			uint16_t tmi;
		case '0' ... '9':
			tmi = strtoul(lp, &tmp, 10);

			if (!vfrom) {
				vfrom = tmi;
			} else {
				vtill = tmi;
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
	} else {
		f = fopen(file, "r");
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

static void
print_cline(cline_t cl, FILE *whither)
{
	if (cl->valid_from == DFLT_FROM && cl->valid_till == DFLT_TILL) {
		/* print nothing */
	} else {
		if (cl->valid_from == DFLT_FROM) {
			fputc('*', whither);
		} else if (cl->valid_from > DFLT_FROM) {
			fprintf(whither, "%u", cl->valid_from);
		} else {
			/* we were meant to fill this bugger */
			abort();
		}
		if (cl->valid_from < cl->valid_till) {
			fputc('-', whither);
			if (cl->valid_till >= DFLT_TILL) {
				fputc('*', whither);
			} else if (cl->valid_from > 0) {
				fprintf(whither, "%u", cl->valid_till);
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
		fputc(' ', whither);
		fprintf(whither, " %04u %.6f", cl->n[i].x, cl->n[i].y);
	}
	fputc('\n', whither);
	return;
}

DEFUN void __attribute__((unused))
print_schema(trsch_t sch, FILE *whither)
{
	for (size_t i = 0; i < sch->np; i++) {
		print_cline(sch->p[i], whither);
	}
	return;
}

DEFUN void
free_cut(trcut_t cut)
{
	free(cut);
	return;
}

DEFUN trcut_t
make_cut(trsch_t sch, idate_t dt)
{
	trcut_t res = NULL;
	idate_t dt_sans = dt % 10000;
	idate_t dt_year = dt / 10000;

	for (size_t i = 0; i < sch->np; i++) {
		struct cline_s *p = sch->p[i];

		/* check year validity */
		if (dt_year < p->valid_from || dt_year > p->valid_till) {
			/* cline isn't applicable */
			continue;
		}
		for (size_t j = 0; j < p->nn - 1; j++) {
			struct cnode_s *n1 = p->n + j;
			struct cnode_s *n2 = n1 + 1;
			idate_t x1 = n1->x;
			idate_t x2 = n2->x;

			if ((dt_sans >= x1 && dt_sans <= x2) ||
			    (x1 > x2 && (dt_sans >= x1 || dt_sans <= x2))) {
				/* something happened between n1 and n2 */
				struct trcc_s cc;
				double xsub = idate_sub(x2, x1);
				double tsub = idate_sub(dt_sans, x1);

				cc.month = p->month;
				cc.year_off = p->year_off;
				cc.y = n1->y + tsub * (n2->y - n1->y) / xsub;
				res = cut_add_cc(res, cc);
				break;
			}
		}
	}
	return res;
}

static void
print_cut(trcut_t c, idate_t dt, double lever, bool rndp, FILE *whither)
{
	char buf[32];

	snprint_idate(buf, sizeof(buf), dt);
	for (size_t i = 0; i < c->ncomps; i++) {
		double expo = c->comps[i].y * lever;

		if (rndp) {
			expo = round(expo);
		}
		fprintf(whither, "%s\t%c%d\t%.4f\n",
			buf,
			c->comps[i].month,
			c->comps[i].year_off + c->year_off,
			expo);
	}
	return;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "truffle-clo.h"
#include "truffle-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	trsch_t sch = NULL;
	trcut_t c;
	int res = 0;

	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	if (argi->schema_given) {
		sch = read_schema(argi->schema_arg);
	} else {
		sch = read_schema("-");
	}
	if (UNLIKELY(sch == NULL)) {
		fputs("schema unreadable\n", stderr);
		res = 1;
		goto sch_out;
	}
	/* finally call our main routine */
	if (argi->inputs_num == 0) {
		print_schema(sch, stdout);
	}
	for (size_t i = 0; i < argi->inputs_num; i++) {
		idate_t dt = read_date(argi->inputs[i], NULL);
		if ((c = make_cut(sch, dt))) {
			double lev = argi->lever_given ? argi->lever_arg : 1.0;
			bool rndp = argi->round_given;

			if (argi->abs_given) {
				c->year_off = dt / 10000;
			}
			print_cut(c, dt, lev, rndp, stdout);
			free_cut(c);
		}
	}
	free_schema(sch);
sch_out:
	cmdline_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* truffle.c ends here */
