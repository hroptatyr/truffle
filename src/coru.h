/*** coru.h -- coroutines
 *
 * Copyright (C) 2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of cattle.
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
#if !defined INCLUDED_coru_h_
#define INCLUDED_coru_h_

#if 0
#include "coru/cocore.h"

typedef struct cocore *coru_t;

#define INIT_CORU_CORE(args...)	initialise_cocore()

#define PREP()			initialise_cocore_thread()
#define UNPREP(args...)		terminate_cocore_thread()
#define INIT(name, ctx)							\
	({								\
		struct cocore *next = (ctx)->next;			\
		create_cocore(						\
			next, (cocore_action_t)(name),			\
			(ctx), sizeof(*(ctx)),				\
			next, 0U, false, 0);				\
	})
#define NEXT1(x, o)							\
	(check_cocore(x) ? switch_cocore((x), (o)) : NULL)
#define NEXT(x)			NEXT1(x, NULL)
#define YIELD1(x, i)		(switch_cocore((x), (i)))
#define YIELD(args...)		YIELD1(coru_ctx->next, (args))
#define NEXT_PACK(x, s, a...)	NEXT1(x, PACK(s, a))

#define DEFCORU(name, out, closure, inargs...)		\
	struct name##_s {				\
		coru_t next;				\
		struct closure args;			\
	};						\
	struct name##_in_s {inargs};			\
	static inline __attribute__((unused))		\
		const struct name##_in_s*		\
	yield_##name(struct cocore *next, out res)	\
	{						\
		return YIELD1(next, &res);		\
	}						\
	static inline __attribute__((unused)) out	\
	next_##name(struct cocore *this,		\
		    struct name##_in_s in)		\
	{						\
		return NEXT1(this, &in);		\
	}						\
	static out					\
	name(const struct name##_s *coru_ctx,		\
	     const struct name##_in_s *arg		\
	     __attribute__((unused)))
#define CORU_CLOSUR(x)	(coru_ctx->args.x)
#define CORU_STRUCT(x)	struct x##_s
#define PACK(x, args...)	&((x){args})
#define INIT_CORU(x, args...)	INIT(x, PACK(CORU_STRUCT(x), args))

#else
#include <coru/coro.h>

typedef coro coru_t;

#define INIT_CORU_CORE(args...)
#define PREP()			coro_init()
#define UNPREP(args...)		coro_free(args)

#define INIT(name, ctx)		coro_new((_entry)name, (cvalue)ctx)
#define NEXT1(x, o)		coro_call((x), (intptr_t)(o))
#define NEXT(x)			NEXT1(x, NULL)
#define YIELD1(x, i)		coro_call((x), (intptr_t)(i))
#define YIELD(args...)		coro_yield((intptr_t)(args))
#define NEXT_PACK(x, s, a...)	NEXT1(x, PACK(s, a))

#define DEFCORU(name, out, closure, inargs...)		\
	struct name##_s {				\
		coru_t next;				\
		struct closure args;			\
	};						\
	struct name##_in_s {inargs};			\
	static out					\
	name(const struct name##_s *coru_ctx,		\
	     const struct name##_in_s *arg		\
	     __attribute__((unused)))
#define CORU_CLOSUR(x)	(coru_ctx->args.x)
#define CORU_STRUCT(x)	struct x##_s
#define PACK(x, args...)	&((x){args})
#define INIT_CORU(x, args...)	INIT(x, PACK(CORU_STRUCT(x), args))

#endif

#endif	/* INCLUDED_coru_h_ */
