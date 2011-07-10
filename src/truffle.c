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
static int
idate_sub(idate_t d1, idate_t d2)
{
	if (d1 == d2) {
		return 0;
	}
	return 1;
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

			if (dt >= n1->x && dt <= n2->x) {
				/* something happened between n1 and n2 */
				struct trcc_s cc;

				cc.month = p->month;
				cc.year_off = p->year_off;
				cc.y = fabs(n2->y - n1->y);
				cc.y /= idate_sub(n2->x, n1->x);
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
	struct cline_s h = {
		.month = 'H',
		.year_off = 0,
		.nn = 4,
		.n = {
			{
				.x = 21207,
				.y = 0.0,
			}, {
				.x = 21208,
				.y = 1.0,
			}, {
				.x = 20307,
				.y = 1.0,
			}, {
				.x = 20308,
				.y = 0.0,
			}
		}
	};
	struct cline_s m = {
		.month = 'M',
		.year_off = 0,
		.nn = 4,
		.n = {
			{
				.x = 20307,
				.y = 0.0,
			}, {
				.x = 20308,
				.y = 1.0,
			}, {
				.x = 20607,
				.y = 1.0,
			}, {
				.x = 20608,
				.y = 0.0,
			}
		}
	};
	struct cline_s u = {
		.month = 'U',
		.year_off = 0,
		.nn = 4,
		.n = {
			{
				.x = 20607,
				.y = 0.0,
			}, {
				.x = 20608,
				.y = 1.0,
			}, {
				.x = 20907,
				.y = 1.0,
			}, {
				.x = 20908,
				.y = 0.0,
			}
		}
	};
	struct cline_s z = {
		.month = 'Z',
		.year_off = 0,
		.nn = 4,
		.n = {
			{
				.x = 20907,
				.y = 0.0,
			}, {
				.x = 20908,
				.y = 1.0,
			}, {
				.x = 21207,
				.y = 1.0,
			}, {
				.x = 21208,
				.y = 0.0,
			}
		}
	};
	trsch_t sch = NULL;
	trcut_t c;

	sch = sch_add_cl(sch, &h);
	sch = sch_add_cl(sch, &m);
	sch = sch_add_cl(sch, &u);
	sch = sch_add_cl(sch, &z);
	c = make_cut(sch, 20404);

	for (size_t i = 0; i < c->ncomps; i++) {
		fprintf(stderr, "%c%d %.4f\n",
			c->comps[i].month, c->comps[i].year_off, c->comps[i].y);
	}

	free_cut(c);
	free_schema(sch);
	return 0;
}
#endif	/* STANDALONE */

/* truffle.c ends here */
