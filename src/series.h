/*** series.h -- read time series of quotes or settlement prices
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
#if !defined INCLUDED_series_h_
#define INCLUDED_series_h_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "daisy.h"
#include "idate.h"
#include "mmy.h"

#if !defined DECLF
# define DECLF		extern
# define DEFUN
#endif	/* !DECLF */

/* we distinguish between oad (once-a-day) and intraday series */
typedef struct trtsc_s *trtsc_t;
typedef const struct trtsc_s *const_trtsc_t;

/* once-a-day series */
struct trtsc_s {
	size_t ndvvs;
	size_t ncons;
	idate_t first;
	idate_t last;
	truf_mmy_t *cons;
	struct __dvv_s *dvvs;
};

struct __dvv_s {
	idate_t d;
	daisy_t dd;
	double *v;
};


/**
 * Read series in truffle format from stream FP. */
DECLF trtsc_t read_series(FILE *fp);

/**
 * Read series in truffle format from FILE. */
DECLF trtsc_t read_series_from_file(const char *file);

/**
 * Free resources associated with series. */
DECLF void free_series(trtsc_t);


static ssize_t
tsc_find_cym_idx(const_trtsc_t s, truf_mmy_t ym)
{
	for (size_t i = 0; i < s->ncons; i++) {
		if (s->cons[i] == ym) {
			return i;
		}
	}
	return -1;
}

#endif	/* INCLUDED_series_h_ */
