#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#if defined STANDALONE
# include <stdio.h>
# define DECLF	static
# define DEFUN	static
#endif	/* STANDALONE */

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
	double y;
};

struct trcut_s {
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
	0, 69, 141, 210, 280, 349, 419, 488, 557, 627, 696, 766, 835
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
		c = malloc(new);
	} else if ((c->ncomps % 16) == 0) {
		size_t new = sizeof(*c) + (c->ncomps + 16) * sizeof(*c->comps);
		c = realloc(c, new);
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
			p = line + strspn(line + 1, skip);
			yoff = 0;
		}
		cl = make_cline(line[0], yoff, 0);

		do {
			dt = strtoul(p, &tmp, 10);
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
	fputc(cl->month, whither);
	if (cl->year_off) {
		fprintf(whither, "%d", cl->year_off);
	}
	for (size_t i = 0; i < cl->nn; i++) {
		fputc(' ', whither);
		fprintf(whither, " %u %.6f", cl->n[i].x, cl->n[i].y);
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

	for (size_t i = 0; i < sch->np; i++) {
		struct cline_s *p = sch->p[i];

		for (size_t j = 0; j < p->nn - 1; j++) {
			struct cnode_s *n1 = p->n + j;
			struct cnode_s *n2 = n1 + 1;

			if (dt >= n1->x && dt <= n2->x) {
				/* something happened between n1 and n2 */
				struct trcc_s cc;
				double xsub = idate_sub(n2->x, n1->x);
				double tsub = idate_sub(dt, n1->x);

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

	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	if (argi->schema_given) {
		sch = read_schema(argi->schema_arg);
	} else {
		sch = read_schema("-");
	}

	/* finally call our main routine */
	if (sch && (c = make_cut(sch, 20020404))) {
		for (size_t i = 0; i < c->ncomps; i++) {
			fprintf(stdout, "%c%d %.4f\n",
				c->comps[i].month,
				c->comps[i].year_off,
				c->comps[i].y);
		}
		free_cut(c);
	}
	if (sch) {
		free_schema(sch);
	}
	cmdline_parser_free(argi);
	return 0;
}
#endif	/* STANDALONE */

/* truffle.c ends here */
