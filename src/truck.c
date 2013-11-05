/*** truffle.c -- tool to apply roll-over directives
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
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
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "truffle.h"
#include "wheap.h"
#include "nifty.h"
#include "coru.h"
#include "trod.h"
#include "mmy.h"
#include "truf-dfp754.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:1572)
#endif /* __INTEL_COMPILER */

struct truf_ctx_s {
	truf_wheap_t q;
};

static truf_trod_t *trods;
static size_t ntrods;
static size_t trodi;


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

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


declcoru(co_echs_rdr, {
		FILE *f;
	});

static const struct co_rdr_res_s {
	echs_instant_t t;
	const char *ln;
	size_t lz;
} *co_echs_rdr(void *UNUSED(arg), const struct co_echs_rdr_initargs_s *c)
{
/* coroutine for the reader of the tseries */
	char *line = NULL;
	size_t llen = 0UL;
	ssize_t nrd;
	/* we'll yield a rdr_res */
	struct co_rdr_res_s res;

	while ((nrd = getline(&line, &llen, c->f)) > 0) {
		char *p;

		if (*line == '#') {
			continue;
		} else if (echs_instant_0_p(res.t = dt_strp(line, &p))) {
			continue;
		} else if (*p != '\t') {
			continue;
		}
		/* pack the result structure */
		res.ln = p + 1U;
		res.lz = nrd - (p + 1U - line);
		yield(res);
	}

	free(line);
	line = NULL;
	llen = 0U;
	return 0;
}

declcoru(co_echs_pop, {
		truf_wheap_t q;
	});

static const struct co_pop_res_s {
	echs_instant_t t;
	truf_trod_t edge;
} *co_echs_pop(void *UNUSED(arg), const struct co_echs_pop_initargs_s *c)
{
/* coroutine for the wheap popper */
	/* we'll yield a pop_res */
	struct co_pop_res_s res;

	while (!echs_instant_0_p(res.t = truf_wheap_top_rank(c->q))) {
		/* assume it's a truf_trod_t */
		uintptr_t tmp = truf_wheap_pop(c->q);
		res.edge = trods[tmp];
		yield(res);
	}
	return 0;
}


/* public api, might go to libtruffle one day */
static int
truf_add_trod(struct truf_ctx_s ctx[static 1U], echs_instant_t t, truf_trod_t d)
{
	uintptr_t qmsg;

	/* resize check */
	if (trodi >= ntrods) {
		size_t nu = ntrods + 64U;
		trods = realloc(trods, nu * sizeof(*trods));
		ntrods = nu;
	}

	/* `clone' D */
	qmsg = (uintptr_t)trodi;
	trods[trodi++] = d;
	/* insert to heap */
	truf_wheap_add_deferred(ctx->q, t, qmsg);
	return 0;
}

static int
truf_read_trod_file(struct truf_ctx_s ctx[static 1U], const char *fn)
{
/* wants a const char *fn */
	coru_t rdr;
	FILE *f;

	if (fn == NULL) {
		f = stdin;
	} else if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
		return -1;
	}

	init_coru();
	rdr = make_coru(co_echs_rdr, f);

	for (const struct co_rdr_res_s *ln; (ln = next(rdr)) != NULL;) {
		/* try to read the whole shebang */
		truf_trod_t c = truf_trod_rd(ln->ln, NULL);
		/* ... and add it */
		truf_add_trod(ctx, ln->t, c);
	}
	/* now sort the guy */
	truf_wheap_fix_deferred(ctx->q);
	fini_coru();
	fclose(f);
	return 0;
}

static void
truf_prnt_trod_file(struct truf_ctx_s ctx[static 1U], FILE *f)
{
	for (echs_instant_t t;
	     !echs_instant_0_p(t = truf_wheap_top_rank(ctx->q));) {
		uintptr_t tmp = truf_wheap_pop(ctx->q);
		truf_trod_t this = trods[tmp];
		char buf[256U];
		char *bp = buf;
		const char *const ep = buf + sizeof(buf);

		bp += dt_strf(bp, ep - bp, t);
		*bp++ = '\t';
		bp += truf_trod_wr(bp, ep - bp, this);
		*bp++ = '\n';
		*bp = '\0';
		fputs(buf, f);
		if (!truf_mmy_p(this.sym) && this.sym) {
			free((char*)this.sym);
		}
	}
	return;
}

static int
truf_appl_tser_file(struct truf_ctx_s ctx[static 1], const char *tser)
{
	static const struct co_pop_res_s nul_ev[1];
	coru_t rdr;
	coru_t pop;
	FILE *f;

	if (UNLIKELY((f = fopen(tser, "r")) == NULL)) {
		return -1;
	}

	init_coru();
	rdr = make_coru(co_echs_rdr, f);
	pop = make_coru(co_echs_pop, ctx->q);

	const struct co_pop_res_s *ev;
	const struct co_rdr_res_s *ln;
	for (ln = next(rdr), ev = next(pop); ln != NULL;) {
		char buf[256U];
		char *bp;
		const char *const ep = buf + sizeof(buf);

		/* sum up caevs in between price lines */
		for (;
		     LIKELY(ev != NULL) &&
			     UNLIKELY(!echs_instant_lt_p(ln->t, ev->t));
		     ev = next(pop)) {
			bp = buf;
			bp += dt_strf(bp, ep - bp, ev->t);
			*bp++ = '\t';
			bp += truf_trod_wr(bp, ep - bp, ev->edge);
			*bp = '\0';
			puts(buf);
		}

		/* apply roll-over directives to price lines */
		do {
			bp = buf;
			bp += dt_strf(bp, ep - bp, ln->t);
			*bp++ = '\t';
			bp += xstrlcpy(bp, ln->ln, ln->lz - 1);
			*bp++ = '\t';
			bp += dt_strf(bp, ep - bp, (ev ?: nul_ev)->t);
			*bp++ = '\t';
			bp += truf_trod_wr(bp, ep - bp, (ev ?: nul_ev)->edge);
			*bp = '\0';
			puts(buf);
		} while (LIKELY((ln = next(rdr)) != NULL) &&
			 (UNLIKELY(ev == NULL) ||
			  LIKELY(echs_instant_lt_p(ln->t, ev->t))));
	}
	fini_coru();
	fclose(f);
	return 0;
}


/* old schema wizardry */
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

static truf_trod_t
make_trod_from_cline(const struct cline_s *p, daisy_t when)
{
	unsigned int y = daisy_to_year(when);
	truf_trod_t res;

	for (size_t j = 0; j < p->nn - 1; j++) {
		const struct cnode_s *n1 = p->n + j;
		const struct cnode_s *n2 = n1 + 1;
		daisy_t l1 = daisy_in_year(n1->l, y);
		daisy_t l2 = daisy_in_year(n2->l, y);

		if (when == l2) {
			/* something happened at l2 */
			if (n2->y == 0.0 && n1->y != 0.0) {
				res.exp = 0.df;
			} else if (n2->y != 0.0 && n1->y == 0.0) {
				res.exp = 1.df;
			} else {
				continue;
			}
		} else if (j == 0 && when == l1) {
			/* something happened at l1 */
			if (UNLIKELY(n1->y != 0.0)) {
				res.exp = 1.df;
			} else {
				continue;
			}
		} else {
			continue;
		}
		res.sym = make_truf_mmy(y + p->year_off, m_to_i(p->month), 0U);

		/* indicate success (as in clear for adding) */
		return res;
	}
	/* indicate failure (to add anything) */
	return truf_nul_trod();
}

static void
bang_schema(struct truf_ctx_s ctx[static 1], trsch_t sch, daisy_t when)
{
	echs_instant_t t = daisy_to_instant(when);

	for (size_t i = 0; i < sch->np; i++) {
		const struct cline_s *p = sch->p[i];
		truf_trod_t d;

		/* check year validity */
		if (when < p->valid_from || when > p->valid_till) {
			/* cline isn't applicable */
			;
		} else if (!((d = make_trod_from_cline(p, when)).sym)) {
			/* nothing added then */
			;
		} else {
			/* just add the guy */
			truf_add_trod(ctx, t, d);
		}
	}
	/* and sort the guy */
	truf_wheap_fix_deferred(ctx->q);
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "truck.xh"
#include "truck.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

static int
cmd_print(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "Usage: truffle print [TROD-FILE]...\n";
	static struct truf_ctx_s ctx[1];
	int res = 0;

	if (argi->inputs_num < 1U) {
		fputs(usg, stderr);
		res = 1;
		goto out;
	} else if (UNLIKELY((ctx->q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	for (unsigned int i = 1U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(ctx, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}
	/* and print him */
	truf_prnt_trod_file(ctx, stdout);

out:
	if (LIKELY(ctx->q != NULL)) {
		free_truf_wheap(ctx->q);
	}
	return res;
}

static int
cmd_migrate(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "Usage: truffle migrate [SCHEMA-FILE]...\n";
	static struct truf_ctx_s ctx[1];
	daisy_t from;
	daisy_t till;
	int res = 0;

	if (argi->inputs_num < 1U) {
		fputs(usg, stderr);
		res = 1;
		goto out;
	} else if (UNLIKELY((ctx->q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	if (argi->from_given) {
		echs_instant_t i = dt_strp(argi->from_arg, NULL);
		from = instant_to_daisy(i);
	} else {
		echs_instant_t i = {2000U, 1U, 1U, ECHS_ALL_DAY};
		from = instant_to_daisy(i);
	}
	if (argi->till_given) {
		echs_instant_t i = dt_strp(argi->till_arg, NULL);
		till = instant_to_daisy(i);
	} else {
		echs_instant_t i = {2037U, 12U, 31U, ECHS_ALL_DAY};
		till = instant_to_daisy(i);
	}

	for (size_t i = 1U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];
		trsch_t sch;

		if ((sch = read_schema(fn)) == NULL) {
			/* try the normal trod reader */
			(void)truf_read_trod_file(ctx, fn);
			continue;
		}
		/* bang into wheap */
		for (daisy_t now = from; now <= till; now++) {
			bang_schema(ctx, sch, now);
		}
		/* and out again */
		free_schema(sch);
	}

	/* and print the whole wheap now */
	truf_prnt_trod_file(ctx, stdout);

out:
	if (LIKELY(ctx->q != NULL)) {
		free_truf_wheap(ctx->q);
	}
	if (LIKELY(trodi > 0U)) {
		free(trods);
		trodi = ntrods = 0U;
	}
	return res;
}

static int
cmd_series(struct truf_args_info argi[static 1U])
{
	static const char usg[] = "\
Usage: truffle series TSER-FILE [TROD-FILE]...\n";
	static struct truf_ctx_s ctx[1];
	int res = 0;

	if (argi->inputs_num < 2U) {
		fputs(usg, stderr);
		res = 1;
		goto out;
	} else if (UNLIKELY((ctx->q = make_truf_wheap()) == NULL)) {
		res = 1;
		goto out;
	}

	for (unsigned int i = 2U; i < argi->inputs_num; i++) {
		const char *fn = argi->inputs[i];

		if (UNLIKELY(truf_read_trod_file(ctx, fn) < 0)) {
			error("cannot open trod file `%s'", fn);
			res = 1;
			goto out;
		}
	}

	with (const char *fn = argi->inputs[1U]) {
		truf_appl_tser_file(ctx, fn);
	}

out:
	if (LIKELY(ctx->q != NULL)) {
		free_truf_wheap(ctx->q);
	}
	return res;
}

int
main(int argc, char *argv[])
{
	struct truf_args_info argi[1];
	int res = 0;

	if (truf_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		goto nocmd;
	}

	/* get the coroutines going */
	init_coru_core();

	/* check the command */
	with (const char *cmd = argi->inputs[0U]) {
		if (!strcmp(cmd, "print")) {
			res = cmd_print(argi);
		} else if (!strcmp(cmd, "migrate")) {
			res = cmd_migrate(argi);
		} else if (!strcmp(cmd, "series")) {
			res = cmd_series(argi);
		} else {
		nocmd:
			error("No valid command specified.\n\
See --help to obtain a list of available commands.");
			res = 1;
			goto out;
		}
	}

out:
	/* just to make sure */
	fflush(stdout);
	truf_parser_free(argi);
	return res;
}

/* truffle.c ends here */
