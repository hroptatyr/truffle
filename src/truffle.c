#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
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


DEFUN void
free_schema(trsch_t sch)
{
	free(sch);
	return;
}

DEFUN trsch_t
read_schema(const char *file)
{
	return NULL;
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

			fprintf(stderr, "%u %u %u\n", n1->x, n2->x, dt);
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
int
main(int argc, char *argv[])
{
	cline_t cl;
	trsch_t sch = NULL;
	trcut_t c;

	cl = make_cline('H', 0, 4);
	cl->n[0].x = 21207;
	cl->n[0].y = 0.0;
	cl->n[1].x = 21208;
	cl->n[1].y = 1.0;
	cl->n[2].x = 20307;
	cl->n[2].y = 1.0;
	cl->n[3].x = 20308;
	cl->n[3].y = 0.0;
	sch = sch_add_cl(sch, cl);

	cl = make_cline('M', 0, 4);
	cl->n[0].x = 20307;
	cl->n[0].y = 0.0;
	cl->n[1].x = 20308;
	cl->n[1].y = 1.0;
	cl->n[2].x = 20607;
	cl->n[2].y = 1.0;
	cl->n[3].x = 20608;
	cl->n[3].y = 0.0;
	sch = sch_add_cl(sch, cl);

	cl = make_cline('U', 0, 4);
	cl->n[0].x = 20607;
	cl->n[0].y = 0.0;
	cl->n[1].x = 20608;
	cl->n[1].y = 1.0;
	cl->n[2].x = 20907;
	cl->n[2].y = 1.0;
	cl->n[3].x = 20908;
	cl->n[3].y = 0.0;
	sch = sch_add_cl(sch, cl);

	cl = make_cline('Z', 0, 4);
	cl->n[0].x = 20907;
	cl->n[0].y = 0.0;
	cl->n[1].x = 20908;
	cl->n[1].y = 1.0;
	cl->n[2].x = 21207;
	cl->n[2].y = 1.0;
	cl->n[3].x = 21208;
	cl->n[3].y = 0.0;
	sch = sch_add_cl(sch, cl);

	/* finally call our main routine */
	if ((c = make_cut(sch, 20020404))) {
		for (size_t i = 0; i < c->ncomps; i++) {
			fprintf(stderr, "%c%d %.4f\n",
				c->comps[i].month, c->comps[i].year_off, c->comps[i].y);
		}
		free_cut(c);
	}

	for (size_t i = 0; i < sch->np; i++) {
		free(sch->p[i]);
	}
	free_schema(sch);
	return 0;
}
#endif	/* STANDALONE */

/* truffle.c ends here */
