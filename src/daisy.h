/*** daisy.h -- daisy dates
 *
 * Copyright (C) 2011-2015 Sebastian Freundt
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
#if !defined INCLUDED_daisy_h_
#define INCLUDED_daisy_h_

#include <stdint.h>
#include "instant.h"

/**
 * Daisy is simply the days since EPOCH. */
typedef uint32_t daisy_t;

#define BASE_YEAR	(1917U)
#define TO_BASE(x)	((x) - BASE_YEAR)
#define TO_YEAR(x)	((x) + BASE_YEAR)

#define DAISY_DIY_BIT	(1 << (sizeof(daisy_t) * 8U - 1U))

/**
 * Convert instant_t to daisy_t */
extern __attribute__((pure, const)) daisy_t instant_to_daisy(echs_instant_t);

/**
 * Convert a daisy_t to an instant_t */
extern __attribute__((pure, const)) echs_instant_t daisy_to_instant(daisy_t);

/**
 * Extract the year off a daisy_t object. */
extern unsigned int daisy_to_year(daisy_t dd);

/**
 * Extract the year off a daisy_t object. */
extern unsigned int daisy_to_year(daisy_t dd);

/**
 * Given a daisy DS, pretend it takes place in year Y. */
extern daisy_t daisy_in_year(daisy_t ds, unsigned int y);

#endif	/* INCLUDED_daisy_h_ */
