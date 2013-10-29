/*** cut.h -- cuts along the date axis
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
#if !defined INCLUDED_cut_h_
#define INCLUDED_cut_h_

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "idate.h"
#include "daisy.h"
#include "schema.h"

#if !defined DECLF
# define DECLF		extern
# define DEFUN
#endif	/* !DECLF */

typedef struct trcut_s *trcut_t;

#define TRCUT_LEVER	(0U)
#define TRCUT_EDGE	(1U)

/* result structure, for cuts etc. */
struct trcc_s {
	uint8_t month;
	uint16_t year;
	union {
		double y;
		uint8_t val;
	};
};

struct trcut_s {
	uint16_t year_off;
	uint16_t type;

	size_t ncomps;
	struct trcc_s comps[];
};

/* printing options */
struct trcut_pr_s {
	/* absolute years instead of relative ones */
	unsigned int abs:1;
	/* use oco notation */
	unsigned int oco:1;
	/* whether to round the output */
	unsigned int rnd:1;
	FILE *out;
	double lever;
};


DECLF trcut_t make_cut(trcut_t, trsch_t schema, daisy_t when);

/**
 * Free resources associated with the cut. */
DECLF void free_cut(trcut_t);

DECLF trcut_t cut_add_cc(trcut_t c, struct trcc_s cc);

DECLF void cut_rem_cc(trcut_t c, struct trcc_s *cc);

DECLF void print_cut(trcut_t c, idate_t i, struct trcut_pr_s);

#endif	/* INCLUDED_cut_h_ */
