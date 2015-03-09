/*** wheap.h -- weak heaps, as stolen from sxemacs
 *
 * Copyright (C) 2007-2015 Sebastian Freundt
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
 **/
#if !defined INCLUDED_wheap_h_
#define INCLUDED_wheap_h_

#include <stdlib.h>
#include "instant.h"

typedef struct truf_wheap_s *truf_wheap_t;


extern truf_wheap_t make_truf_wheap(void);
extern void free_truf_wheap(truf_wheap_t);

extern echs_instant_t truf_wheap_top_rank(truf_wheap_t);
extern uintptr_t truf_wheap_top(truf_wheap_t);
extern uintptr_t truf_wheap_pop(truf_wheap_t);

extern void truf_wheap_add(truf_wheap_t, echs_instant_t, uintptr_t);

/**
 * Bulk inserts. */
extern void truf_wheap_add_deferred(truf_wheap_t, echs_instant_t, uintptr_t);
/**
 * Recreate the heap property after deferred inserts. */
extern void truf_wheap_fix_deferred(truf_wheap_t);

/**
 * Sort the entire heap. */
extern void truf_wheap_sort(truf_wheap_t);

#endif	/* INCLUDED_wheap_h_ */
