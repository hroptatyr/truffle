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
#include "truf-dfp754.h"

struct truf_ctx_s {
	truf_wheap_t q;
};


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


struct co_rdr_res_s {
	echs_instant_t t;
	const char *ln;
	size_t lz;
};

DEFCORU(co_rdr, const struct co_rdr_res_s*, {
		FILE *f;
	})
{
/* coroutine for the reader of the tseries */
#define yield(args...)	yield_co_rdr(args)
	char *line = NULL;
	size_t llen = 0UL;
	ssize_t nrd;
	/* we'll yield a rdr_res */
	static struct co_rdr_res_s res;

	while ((nrd = getline(&line, &llen, CORU_CLOSUR(f))) > 0) {
		char *p;

		if (*line == '#') {
			continue;
		} else if ((p = strchr(line, '\t')) == NULL) {
			break;
		} else if (echs_instant_0_p(res.t = dt_strp(line, NULL))) {
			break;
		}
		/* pack the result structure */
		res.ln = p + 1U;
		res.lz = nrd - (p + 1U - line);
		YIELD(&res);
	}

	free(line);
	line = NULL;
	llen = 0U;
#undef yield
	return 0;
}


/* public api, might go to libtruffle one day */
static truf_trod_t
truf_trod_rd(const char msg[static 1U])
{
	static const char sep[] = " \t\n";
	const char *brk;
	truf_trod_t res;

	if (!*(brk += strcspn(brk = msg, sep))) {
		return truf_nul_trod();
	}
	res.sym = strndup(msg, brk - msg);
	res.exp = strtod32(brk + 1U, NULL);
	return res;
}

static size_t
truf_trod_wr(char *restrict buf, size_t bsz, truf_trod_t t)
{
	char *restrict bp = buf;
	const char *const ep = bp + bsz;

	if (LIKELY(t.sym != NULL)) {
		bp += xstrlcpy(buf, t.sym, bsz);
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

static int
truf_read_trod_file(struct truf_ctx_s ctx[static 1U], const char *fn)
{
/* wants a const char *fn */
	static truf_trod_t *trods;
	static size_t ntrods;
	size_t trodi = 0U;
	struct cocore *rdr;
	struct cocore *me;
	FILE *f;

	if (fn == NULL) {
		f = stdin;
	} else if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
		return -1;
	}

	me = PREP();
	rdr = INIT_CORU(co_rdr, .args.f = f, .next = me);

	for (const struct co_rdr_res_s *ln; (ln = NEXT(rdr)) != NULL;) {
		/* try to read the whole shebang */
		truf_trod_t c = truf_trod_rd(ln->ln);
		uintptr_t qmsg;

		/* resize check */
		if (trodi >= ntrods) {
			size_t nu = ntrods + 64U;
			trods = realloc(trods, nu * sizeof(*trods));
			ntrods = nu;
		}

		/* `clone' C */
		qmsg = (uintptr_t)(trods + trodi);
		trods[trodi++] = c;
		/* insert to heap */
		truf_wheap_add_deferred(ctx->q, ln->t, qmsg);
	}
	/* now sort the guy */
	truf_wheap_fix_deferred(ctx->q);
	fclose(f);
	UNPREP();
	return 0;
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

	for (echs_instant_t t;
	     !echs_instant_0_p(t = truf_wheap_top_rank(ctx->q));) {
		uintptr_t tmp = truf_wheap_pop(ctx->q);
		const truf_trod_t *this = (const void*)tmp;
		char buf[256U];
		char *bp = buf;
		const char *const ep = buf + sizeof(buf);

		bp += dt_strf(bp, ep - bp, t);
		*bp++ = '\t';
		bp += truf_trod_wr(bp, ep - bp, *this);
		*bp++ = '\n';
		*bp = '\0';
		fputs(buf, stdout);
		free(deconst(this->sym));
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
	initialise_cocore();

	/* check the command */
	with (const char *cmd = argi->inputs[0U]) {
		if (!strcmp(cmd, "print")) {
			res = cmd_print(argi);
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
