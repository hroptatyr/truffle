/*** rpaf.h -- reference prices and accrued flows
 *
 * Copyright (C) 2009-2020 Sebastian Freundt
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
#if !defined INCLUDED_rpaf_h_
#define INCLUDED_rpaf_h_

#include "truffle.h"
#include "step.h"

typedef struct truf_rpaf_s truf_rpaf_t;

struct truf_rpaf_s {
	truf_price_t refprc;
	truf_price_t cruflo;
};


/**
 * Apply ST to intrinsic state stored in the rpaf pool and return the flow.
 * Use this routine to do the summing yourself. */
extern truf_rpaf_t truf_rpaf_step(const struct truf_step_s st[static 1U]);

/**
 * Apply ST to intrinsic state stored in the rpaf pool and return accrued flow.
 * Use this routine to obtain an accumulating picture. */
extern truf_rpaf_t truf_rpaf_scru(const struct truf_step_s st[static 1U]);

extern void truf_init_rpaf(void);
extern void truf_fini_rpaf(void);

#endif	/* INCLUDED_rpaf_h_ */
