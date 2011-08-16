#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/mman.h>

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
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*(x)))
#endif	/* !countof */

typedef uint32_t idate_t;
typedef uint32_t daysi_t;
typedef struct trcut_s *trcut_t;
typedef struct trsch_s *trsch_t;
typedef struct cline_s *cline_t;
typedef struct trser_s *trser_t;
typedef struct trtsc_s *trtsc_t;

#define DAYSI_DIY_BIT	(1 << (sizeof(daysi_t) * 8 - 1))

/* a node */
struct cnode_s {
	idate_t x;
	daysi_t l;
	double y __attribute__((aligned(sizeof(double))));
};

/* a single line */
#define DFLT_FROM	(0)
#define DFLT_TILL	(1048576)
struct cline_s {
	daysi_t valid_from;
	daysi_t valid_till;
	char month;
	int8_t year_off;
	size_t nn;
	struct cnode_s n[];
} __attribute__((aligned(sizeof(void*))));

/* schema */
struct trsch_s {
	size_t np;
	struct cline_s *p[];
};

/* result structure, for cuts etc. */
struct trcc_s {
	char month;
	uint16_t year;
	double y __attribute__((aligned(16)));
};

struct trcut_s {
	uint16_t year_off;

	size_t ncomps;
	struct trcc_s comps[];
};

struct __dv_s {
	idate_t d;
	double v;
};

struct __dvv_s {
	idate_t d;
	daysi_t dd;
	double *v;
};

struct trtsc_s {
	size_t ndvvs;
	size_t ncons;
	idate_t first;
	idate_t last;
	uint32_t *cons;
	struct __dvv_s *dvvs;
};
#define TSC_STEP	(4096)
#define CYM_STEP	(256)

DECLF trcut_t make_cut(trsch_t schema, daysi_t when);
DECLF void free_cut(trcut_t);

DECLF trsch_t read_schema(const char *file);
DECLF void free_schema(trsch_t);


/* alloc helpers */
#define PROT_MEM		(PROT_READ | PROT_WRITE)
#define MAP_MEM			(MAP_PRIVATE | MAP_ANONYMOUS)

static inline int
resize_mmap(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	if (cnt == 0) {
		size_t new = inc * blksz;
		*ptr = mmap(NULL, new, PROT_MEM, MAP_MEM, 0, 0);
		return 1;

	} else if (cnt % inc == 0) {
		/* resize */
		size_t old = cnt * blksz;
		size_t new = (cnt + inc) * blksz;
		*ptr = mremap(*ptr, old, new, MREMAP_MAYMOVE);
		return 1;
	}
	return 0;
}

static inline void
unsize_mmap(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	size_t sz;

	if (*ptr == NULL || cnt == 0) {
		return;
	}

	sz = (cnt / inc + 1) * inc * blksz;
	munmap(*ptr, sz);
	*ptr = NULL;
	return;
}

static inline void
upsize_mmap(void **ptr, size_t cnt, size_t cnn, size_t blksz, size_t inc)
{
/* like resize, but definitely provide space for cnt + inc objects */
	size_t old = (cnt / inc + 1) * inc * blksz;
	size_t new = (cnn / inc + 1) * inc * blksz;

	if (*ptr == NULL) {
		*ptr = mmap(NULL, new, PROT_MEM, MAP_MEM, 0, 0);
	} else if (old < new) {
		*ptr = mremap(*ptr, old, new, MREMAP_MAYMOVE);
	}
	return;
}

static inline int
resize_mall(void **ptr, size_t cnt, size_t blksz, size_t inc)
{
	if (cnt == 0) {
		*ptr = calloc(inc, blksz);
		return 1;
	} else if (cnt % inc == 0) {
		/* resize */
		size_t new = (cnt + inc) * blksz;
		*ptr = realloc(*ptr, new);
		return 1;
	}
	return 0;
}

static inline void
unsize_mall(void **ptr, size_t cnt, size_t UNUSED(blksz), size_t UNUSED(inc))
{
	if (*ptr == NULL || cnt == 0) {
		return;
	}

	free(*ptr);
	*ptr = NULL;
	return;
}

static inline void
upsize_mall(void **ptr, size_t cnt, size_t cnn, size_t blksz, size_t inc, int f)
{
/* like resize, but definitely provide space for cnt + inc objects */
	size_t old = (cnt / inc + 1) * inc * blksz;
	size_t new = (cnn / inc + 1) * inc * blksz;
	if (*ptr == NULL) {
		*ptr = malloc(new);
		memset(*ptr, f, new);
	} else if (old < new) {
		*ptr = realloc(*ptr, new);
		memset((char*)*ptr + old, f, new - old);
	}
	return;
}


/* idate helpers */
static uint16_t dm[] = {
/* this is \sum ml, first element is a bit set of leap days to add */
	0xfff8, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

static uint32_t ds_sum[256];

#define BASE_YEAR	(1917)
#define TO_BASE(x)	((x) - BASE_YEAR)
#define TO_YEAR(x)	((x) + BASE_YEAR)

#if 0
/* standalone version and adapted to what make_cut() needs */
static int
daysi_to_year(daysi_t ddt)
{
/* given days since 1917-01-01 (Mon), compute a year */
	int y;

	if (UNLIKELY(ddt & DAYSI_DIY_BIT)) {
		return 0;
	}

	for (y = ddt / 365; (y * 365 + y / 4) >= ddt; y--);
	return TO_YEAR(y);
}
#elif 1
static int
daysi_to_year(daysi_t ddt)
{
/* given days since 1917-01-01 (Mon), compute a year */
	if (UNLIKELY(ddt & DAYSI_DIY_BIT)) {
		return 0;
	}

	/* scan target */
	for (int y = TO_BASE(1980); y < TO_BASE(2020); y++) {
		if (ds_sum[y] > ddt) {
			return TO_YEAR(y - 1);
		}
	}
	for (int y = 0; y < TO_BASE(1980); y++) {
		if (ds_sum[y] > ddt) {
			return TO_YEAR(y - 1);
		}
	}
	for (int y = TO_BASE(2020); y < countof(ds_sum); y++) {
		if (ds_sum[y] > ddt) {
			return TO_YEAR(y - 1);
		}
	}
	return 0;
}
#endif

/* standalone version, we could use ds_sum but this is most likely
 * causing more cache misses */
static idate_t
daysi_to_idate(daysi_t ddt)
{
/* given days since 1917-01-01 (Mon),
 * compute the idate_t representation X so that idate_to_daysi(X) == DDT */
	int m;
	int d;
	int y;

	if (LIKELY(ddt & DAYSI_DIY_BIT)) {
		ddt &= ~DAYSI_DIY_BIT;
	}
	y = daysi_to_year(ddt);

	for (m = 1; m < 12 && ddt > dm[m + 1]; m++);
	d = ddt - dm[m];

	if (UNLIKELY(y > 0 && y % 4 == 0)) {
		if ((dm[0] >> (m)) & 1) {
			if (UNLIKELY(ddt == 60)) {
				m = 2;
				d = 29;
			} else if (UNLIKELY(ddt == dm[m] + 1)) {
				m--;
				d = ddt - dm[m] - 1;
			} else {
				d--;
			}
		}
	}
	return y * 10000 + m * 100 + d;
}

static daysi_t
idate_to_daysi(idate_t dt)
{
/* compute days since 1917-01-01 (Mon),
 * if year slot is absent in D compute the day in the year of D instead. */
	int y = dt / 10000;
	int m = (dt / 100) % 100;
	int d = dt % 100;
	daysi_t res = dm[m] + d;

	if (LIKELY(y == 0)) {
		res |= DAYSI_DIY_BIT;
	} else {
		int dy = (y - BASE_YEAR);

		res += dy * 365 + dy / 4;
		if (UNLIKELY(dy % 4 == 3)) {
			res += (dm[0] >> (m)) & 1;
		}
	}
	return res;
}

static daysi_t
daysi_in_year(daysi_t ds, int y)
{
	int off = y - BASE_YEAR;

	if (UNLIKELY(!(ds & DAYSI_DIY_BIT))) {
		/* we could technically do something here */
		;
	} else if (UNLIKELY(off < 0 || off > countof(ds_sum))) {
		return 0U;
	} else {
		ds &= ~DAYSI_DIY_BIT;
		if (UNLIKELY(y % 4 == 0) && ds >= 60) {
			ds++;
		}
		ds += ds_sum[off] - 1;
	}
	return ds;
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
make_cline(char month, int8_t yoff)
{
	cline_t res = malloc(sizeof(*res));

	res->month = month;
	res->year_off = yoff;
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
	cl->n[idx].l = idate_to_daysi(x);
	return cl;
}

static char*
__p2c(const char *str)
{
	union {
		const char *c;
		char *p;
	} this = {.c = str};
	return this.p;
}

static idate_t
read_date(const char *str, char **restrict ptr)
{
#define C(x)	(x - '0')
	idate_t res = 0;
	const char *tmp;

	tmp = str;
	res = C(tmp[0]) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (*tmp == '\0' || isspace(*tmp)) {
		if (ptr) {
			*ptr = __p2c(tmp);
		}
		return res;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (*tmp == '\0' || isspace(*tmp)) {
		if (ptr) {
			*ptr = __p2c(tmp);
		}
		return res;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2 + (tmp[2] == '-');

	if (*tmp == '\0' || isspace(*tmp)) {
		/* date is fucked? */
		if (ptr) {
			*ptr = __p2c(tmp);
		}
		return 0;
	}

	res = (res * 10 + C(tmp[0])) * 10 + C(tmp[1]);
	tmp = tmp + 2;

	if (ptr) {
		*ptr = __p2c(tmp);
	}
#undef C
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

static void
__err_not_asc(const char *line, size_t llen)
{
	fputs("error: ", stderr);
	fwrite(line, llen, 1, stderr);
	fputs("error: dates are not in ascending order\n", stderr);
	return;
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
		cl = make_cline(line[0], yoff);

		do {
			dt = read_date(p, &tmp) % 10000;
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
			daysi_t ddt = idate_to_daysi(dt);
			if (cl->nn && ddt <= cl->n[cl->nn - 1].l) {
				__err_not_asc(line, llen);
				free(cl);
				return NULL;
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
}

static cline_t
read_schema_line(const char *line, size_t UNUSED(llen))
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
make_cut(trsch_t sch, daysi_t when)
{
	trcut_t res = NULL;
	int y = daysi_to_year(when);

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
				cc.year = y + p->year_off;
				cc.y = n1->y + tsub * ysub / xsub;
				res = cut_add_cc(res, cc);
				break;
			}
		}
	}
	return res;
}

static int
m_to_i(char month)
{
	switch (month) {
	case 'F':
		return 1;
	case 'G':
		return 2;
	case 'H':
		return 3;
	case 'J':
		return 4;
	case 'K':
		return 5;
	case 'M':
		return 6;
	case 'N':
		return 7;
	case 'Q':
		return 8;
	case 'U':
		return 9;
	case 'V':
		return 10;
	case 'X':
		return 11;
	case 'Z':
		return 12;
	default:
		return 0;
	}
}

static void
print_cut(trcut_t c, idate_t dt, double lever, bool rnd, bool oco, FILE *out)
{
	char buf[32];
	int y = dt / 10000;

	snprint_idate(buf, sizeof(buf), dt);
	for (size_t i = 0; i < c->ncomps; i++) {
		double expo = c->comps[i].y * lever;

		if (rnd) {
			expo = round(expo);
		}
		if (!oco) {
			fprintf(out, "%s\t%c%d\t%.8g\n",
				buf,
				c->comps[i].month,
				c->comps[i].year - y + c->year_off,
				expo);
		} else {
			fprintf(out, "%s\t%d%02d\t%.8g\n",
				buf,
				c->comps[i].year - y + c->year_off,
				m_to_i(c->comps[i].month),
				expo);
		}
	}
	return;
}


/* series handling */
static uint32_t
cym_to_ym(char month, uint16_t year)
{
	return (year << 8) + (uint8_t)month;
}

static ssize_t
tsc_find_cym_idx(trtsc_t s, uint32_t ym)
{
	for (ssize_t i = 0; i < s->ncons; i++) {
		if (s->cons[i] == ym) {
			return i;
		}
	}
	return -1;
}

static struct __dvv_s*
tsc_find_dvv(trtsc_t s, idate_t dt)
{
	for (size_t i = 0; i < s->ndvvs; i++) {
		if (s->dvvs[i].d == dt) {
			return s->dvvs + i;
		}
	}
	return NULL;
}

static ssize_t
tsc_find_dvv_idx(trtsc_t s, idate_t dt)
{
/* find first index where dvv date >= dt */
	for (size_t i = 0; i < s->ndvvs; i++) {
		if (s->dvvs[i].d >= dt) {
			return i;
		}
	}
	return -1;
}

static void
tsc_move(trtsc_t s, ssize_t idx, int num)
{
/* move dvv vector from index IDX onwards so that NUM dvvs fit in-between */
	void **tmp = (void**)&s->dvvs;
	size_t nmov;
	size_t nndvvs = s->ndvvs + num;

	upsize_mmap(tmp, s->ndvvs, nndvvs, sizeof(*s->dvvs), TSC_STEP);
	nmov = (s->ndvvs - idx) * sizeof(*s->dvvs);
	memmove(s->dvvs + idx + num, s->dvvs + idx, nmov);
	memset(s->dvvs + idx, 0, num * sizeof(*s->dvvs));
	s->ndvvs = nndvvs;
	return;
}

static struct __dvv_s*
tsc_init_dvv(trtsc_t s, size_t idx, idate_t dt)
{
	struct __dvv_s *t = s->dvvs + idx;
	t->d = dt;
	/* make room for s->ncons doubles and set them to nan */
	upsize_mall((void**)&t->v, 0, s->ncons, sizeof(*t->v), CYM_STEP, -1);
	return t;
}

static void
tsc_add_dv(trtsc_t s, char mon, uint16_t yoff, struct __dv_s dv)
{
	struct __dvv_s *this = NULL;
	ssize_t idx = 0;

	/* find the date in question first */
	if (dv.d > s->last) {
		/* append */
		void **tmp = (void**)&s->dvvs;
		resize_mmap(tmp, s->ndvvs, sizeof(*s->dvvs), TSC_STEP);
		this = tsc_init_dvv(s, s->ndvvs++, dv.d);
		/* update stats */
		s->last = dv.d;
		if (UNLIKELY(s->first == 0)) {
			s->first = dv.d;
		}
	} else if ((this = tsc_find_dvv(s, dv.d)) != NULL) {
		/* bingo */
		;
	} else if (dv.d < s->first ||
		   (idx = tsc_find_dvv_idx(s, dv.d)) > 0) {
		/* prepend, FUCK */
		tsc_move(s, idx, 1);
		this = tsc_init_dvv(s, idx, dv.d);
		if (UNLIKELY(dv.d < s->first)) {
			s->first = dv.d;
		}
		fputs("\
warning: unsorted input data will result in poor performance\n", stderr);
	} else {
		abort();
	}

	/* now find the cmy offset */
	if ((idx = tsc_find_cym_idx(s, cym_to_ym(mon, yoff))) < 0) {
		/* append symbol */
		void **tmp = (void**)&s->cons;
		if (resize_mall(tmp, s->ncons, sizeof(*s->cons), CYM_STEP)) {
			for (size_t i = 0; i < s->ndvvs; i++) {
				upsize_mall(
					(void**)&s->dvvs[i].v, 0, s->ncons,
					sizeof(*s->dvvs[i].v), CYM_STEP, -1);
			}
		}
		idx = s->ncons++;
		s->cons[idx] = cym_to_ym(mon, yoff);
	}
	/* resize the double vector maybe */
	this->v[idx] = dv.v;
	return;
}

static void
__tsc_fixup(trtsc_t s)
{
/* go through the series again and fix up holes */
	/* check first guy first */
	for (size_t j = 0; j < s->ncons; j++) {
		if (isnan(s->dvvs[0].v[j])) {
			s->dvvs[0].v[j] = 0.0;
		}
	}
	for (size_t i = 1; i < s->ndvvs; i++) {
		double *old = s->dvvs[i - 1].v;
		double *this = s->dvvs[i].v;

		for (size_t j = 0; j < s->ncons; j++) {
			if (isnan(this[j])) {
				this[j] = old[j];
			}
		}
	}
	return;
}

static trtsc_t
read_series(FILE *f)
{
	trtsc_t res = NULL;
	size_t llen = 0UL;
	char *line = NULL;

	/* get us some container */
	res = calloc(1, sizeof(*res));
	/* read the series file first */
	while (getline(&line, &llen, f) > 0) {
		char *con = line;
		char *dat;
		char *val;
		char mon;
		uint16_t yoff;
		struct __dv_s dv;

		if ((dat = strchr(con, '\t')) == NULL) {
			break;
		}
		if (!(dv.d = read_date(dat + 1, &val)) ||
		    (val == NULL) ||
		    (dv.v = strtod(val + 1, &val), val) == NULL) {
			break;
		}

		mon = con[0];
		yoff = strtoul(con + 1, NULL, 10);
		tsc_add_dv(res, mon, yoff, dv);
	}
	if (line) {
		free(line);
	}
	__tsc_fixup(res);
	return res;
}

static void
free_series(trtsc_t s)
{
	for (size_t i = 0; i < s->ndvvs; i++) {
		void **tmp = (void**)&s->dvvs[i].v;
		unsize_mall(tmp, s->ncons, sizeof(double), CYM_STEP);
	}
	unsize_mall((void**)&s->cons, s->ncons, sizeof(*s->cons), CYM_STEP);
	unsize_mmap((void**)&s->dvvs, s->ndvvs, sizeof(*s->dvvs), TSC_STEP);
	free(s);
	return;
}

static double
cut_flow(trcut_t c, idate_t dt, trtsc_t tsc, double tick_val, bool init)
{
	double res = 0;
	double *new_v = NULL;
	double *old_v = NULL;

	for (size_t i = 0; i < tsc->ndvvs; i++) {
		if (tsc->dvvs[i].d == dt) {
			new_v = tsc->dvvs[i].v;
			old_v = i > 0 ? tsc->dvvs[i - 1].v : NULL;
			break;
		}
	}
	for (size_t i = 0; i < c->ncomps; i++) {
		double expo = c->comps[i].y * tick_val;
		char mon = c->comps[i].month;
		uint16_t year = c->comps[i].year;
		uint32_t ym = cym_to_ym(mon, year);
		ssize_t idx;

		if (expo == 0.0) {
			/* don't bother */
			continue;
		} else if ((idx = tsc_find_cym_idx(tsc, ym)) < 0) {
#if 1
			fprintf(stderr, "\
cut contained %c%u %.8g but no quotes have been found\n", mon, year, expo);
#endif	/* 0 */
			continue;
		}
		if (LIKELY(old_v != NULL && !init)) {
			res += expo * (new_v[idx] - old_v[idx]);
		} else {
			res += expo * new_v[idx];
		}
	}
	return res;
}

static double
cut_base(trcut_t c, idate_t dt, trtsc_t tsc, double tick_val, double base)
{
	double res = 0;
	double *new_v = NULL;

	for (size_t i = 0; i < c->ncomps; i++) {
		double expo = c->comps[i].y * tick_val;
		char mon = c->comps[i].month;
		uint16_t year = c->comps[i].year;
		uint32_t ym = cym_to_ym(mon, year);
		ssize_t idx;

		if (expo == 0.0) {
			/* don't bother */
			continue;
		} else if ((idx = tsc_find_cym_idx(tsc, ym)) < 0) {
#if 1
			fprintf(stderr, "\
cut contained %c%u %.8g but no quotes have been found\n", mon, year, expo);
#endif	/* 0 */
			continue;
		}
		res += expo * (new_v[idx] - base);
	}
	return res;
}

struct __series_spec_s {
	const char *ser_file;
	double tick_val;
	double basis;
	bool cump;
};

static void
roll_series(trsch_t s, struct __series_spec_s ser_sp, FILE *whither)
{
	trtsc_t ser;
	FILE *f;
	double anchor = 0.0;
	double old_an = NAN;
	trcut_t c;

	if ((f = fopen(ser_sp.ser_file, "r")) == NULL) {
		fprintf(stderr, "could not open file %s\n", ser_sp.ser_file);
		return;
	} else if ((ser = read_series(f)) == NULL) {
		return;
	}

	/* find the earliest date */
	for (size_t i = 0; i < ser->ndvvs; i++) {
		idate_t dt = ser->dvvs[i].d;
		daysi_t mc_ds = idate_to_daysi(dt);

		/* anchor now contains the very first date and value */
		if ((c = make_cut(s, mc_ds - /*yday*/!ser_sp.cump))) {
			char buf[32];
			double cf;
			bool init = isnan(old_an);

			cf = cut_flow(c, dt, ser, ser_sp.tick_val, init);
			if (ser_sp.cump) {
				if (UNLIKELY(isnan(old_an))) {
					if (LIKELY(cf == 0.0)) {
						goto leave_cut;
					}
					anchor = cf;
				} else {
					anchor = old_an + cf;
				}
			} else {
				anchor = cf;
			}
			snprint_idate(buf, sizeof(buf), dt);
			fprintf(whither, "%s\t%.8g\n", buf, anchor);
			old_an = anchor;
		leave_cut:
			free_cut(c);
		}
	}

	/* free up resources */
	if (ser) {
		free_series(ser);
	}
	fclose(f);
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

	/* initialise the daysi sums */
	for (size_t i = 0; i < countof(ds_sum); i++) {
		ds_sum[i] = idate_to_daysi((i + BASE_YEAR) * 10000 + 101);
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
	if (argi->series_given) {
		struct __series_spec_s sp = {
			.ser_file = argi->series_arg,
			.tick_val = argi->tick_value_given
			? argi->tick_value_arg : 1.0,
			.basis = argi->basis_given
			? argi->basis_arg : 0.0,
			.cump = !argi->flow_given,
		};
		roll_series(sch, sp, stdout);
	} else if (argi->inputs_num == 0) {
		print_schema(sch, stdout);
	} else {
		double lev = argi->lever_given ? argi->lever_arg : 1.0;
		bool rndp = argi->round_given;
		bool ocop = argi->oco_given;

		for (size_t i = 0; i < argi->inputs_num; i++) {
			idate_t dt = read_date(argi->inputs[i], NULL);
			daysi_t ds = idate_to_daysi(dt);

			if ((c = make_cut(sch, ds))) {
				if (argi->abs_given) {
					c->year_off = dt / 10000;
				}
				print_cut(c, dt, lev, rndp, ocop, stdout);
				free_cut(c);
			}
		}
	}
	free_schema(sch);
sch_out:
	cmdline_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* truffle.c ends here */
