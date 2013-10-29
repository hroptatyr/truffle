/*** idate.h -- integer coded dates
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
#if !defined INCLUDED_idate_h_
#define INCLUDED_idate_h_

#include <stdint.h>
#include <stdlib.h>
#include "daisy.h"

/**
 * Daisy is simply the days since EPOCH. */
typedef uint32_t idate_t;

/**
 * Turn STR into idate and set ON to character after the last consumed one. */
extern idate_t read_idate(const char *str, char **restrict on);

/**
 * Turn idate I to string in BUF of size BSZ. */
extern size_t prnt_idate(char *restrict buf, size_t bsz, idate_t i);

/**
 * Special purpose idate->daisy glue. */
extern __attribute__((pure, const)) daisy_t daisy_sans_year(idate_t id);

/**
 * Convert daisy object to idate object. */
extern __attribute__((pure, const)) idate_t daisy_to_idate(daisy_t dd);

/**
 * Convert idate object to daisy object */
extern __attribute__((pure, const)) daisy_t idate_to_daisy(idate_t dt);


static inline __attribute__((pure, const)) unsigned int
idate_y(idate_t dt)
{
	return dt / 10000U;
}

static inline __attribute__((pure, const)) unsigned int
idate_m(idate_t dt)
{
	return (dt % 10000U) / 100U;
}

static inline __attribute__((pure, const)) unsigned int
idate_d(idate_t dt)
{
	return dt % 100U;
}

#endif	/* INCLUDED_idate_h_ */
