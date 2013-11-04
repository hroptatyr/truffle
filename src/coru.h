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

static __thread coru_t ____caller;

#define init_coru_core(args...)	initialise_cocore()
#define init_coru()		____caller = initialise_cocore_thread()
#define fini_coru()		terminate_cocore_thread(), ____caller = NULL

#define declcoru(name, init...)	struct name##_initargs_s init

#define make_coru(x, init...)						\
	({								\
		struct x##_initargs_s __initargs = {init};		\
		create_cocore(						\
			____caller, (cocore_action_t)(x),		\
			&__initargs, sizeof(__initargs),		\
			____caller, 0U, false, 0);			\
	})								\

#define next(x)			____next(x, NULL)
#define next_with(x, val)				\
	({						\
		static typeof((val)) ____res;		\
							\
		____res = val;				\
		____next(x, &____res);			\
	})
#define ____next(x, ptr)				\
	(check_cocore(x)				\
	 ? switch_cocore((x), ptr)			\
	 : NULL)

#define yield(yld)				\
	({					\
		coru_t tmp = ____caller;	\
		yield_to(tmp, yld);		\
	})
#define yield_to(x, yld)				\
	({						\
		static typeof((yld)) ____res;		\
							\
		____res = yld;				\
		switch_cocore((x), (void*)&____res);	\
	})

#else
/* my own take on things */
#include <setjmp.h>
#include <stdint.h>
#include <assert.h>

typedef struct {
	void *stk;
	size_t ssz;
	jmp_buf *jb;
} coru_t;

#define init_coru_core(args...)
#define init_coru()
#define fini_coru()

static __thread coru_t ____caller;
static __thread coru_t ____callee;
static intptr_t ____glob;
static void *sch_rbp;

#if !defined _setjmp
# define _setjmp		setjmp
#endif	/* !_setjmp */
#if !defined _longjmp
# define _longjmp		longjmp
#endif	/* !_longjmp */

#define declcoru(name, init...)	struct name##_initargs_s init

#define make_coru(x, init...)						\
	({								\
		static jmp_buf __##x##b;				\
		struct x##_initargs_s __initargs = {init};		\
									\
		/* keep track of frame */				\
		if (_setjmp(__##x##b)) {				\
			char *buf = alloca(0x4000U);			\
			asm volatile("" :: "m" (buf));			\
			printf("calling " #x " %p\n", sch_rbp);		\
			x((void*)____glob, &__initargs);		\
			puts(# x " returning");				\
			____glob = NULL;				\
			longjmp(*____caller.jb, 1);			\
		}							\
		(coru_t){.jb = &__##x##b};				\
	})

#define stash(x)							\
	({								\
		void *rbp = (char*)__builtin_frame_address(0) - 0x100U;	\
		size_t ssz = (char*)sch_rbp - (char*)rbp;		\
									\
		if (sch_rbp > rbp) {					\
			if (ssz > x.ssz) {				\
				x.stk = realloc(x.stk, x.ssz);		\
				x.ssz = ssz;				\
			}						\
			printf("stashing %p: %p %zu -> %p %zu\n",	\
			       x.jb, rbp, ssz, x.stk, x.ssz);		\
			memcpy(x.stk, rbp, ssz);			\
		}							\
	})
#define rstor(x)							\
	({								\
		printf("restoring %p: %p %zu <- %p %zu\n",		\
		       x.jb, (char*)sch_rbp - x.ssz, x.ssz,		\
		       x.stk, x.ssz);					\
		if (x.ssz) {						\
			memcpy((char*)sch_rbp - x.ssz, x.stk, x.ssz);	\
		}							\
	})

#define yield(yld)				\
	({					\
		coru_t tmp = ____caller;	\
		yield_to(tmp, yld);		\
	})

#define yield_to(x, yld)						\
	({								\
		static typeof((yld)) ____res;				\
									\
		____res = yld;						\
		____glob = (intptr_t)&____res;				\
		if (!_setjmp(*____callee.jb)) {				\
			/* save the stack */				\
			stash(____callee);				\
			____caller = ____callee;			\
			____callee = x;					\
			_longjmp(*x.jb, 1);				\
			assert(0);					\
		}							\
		/* restore the stack */					\
		rstor(____callee);					\
		(void*)____glob;					\
	})

#define next(x)			next_with(x, NULL)

#define next_with(x, val)			\
	({								\
		static jmp_buf __##x##sb;				\
		static typeof((val)) ____res;				\
									\
		____res = val;						\
		____glob = (intptr_t)&____res;				\
		/* keep track of frame */				\
		sch_rbp = (char*)__builtin_frame_address(0) - 0x1000U;	\
		printf("sch_rbp %p\n", sch_rbp);			\
		if (!_setjmp(__##x##sb)) {				\
			____caller = (coru_t){.jb = &__##x##sb};	\
			____callee = x;					\
			stash(____caller);				\
			printf("calling %p\n", x.jb);			\
			_longjmp(*x.jb, 1);				\
		}							\
		/* restore __##x##sb */					\
		rstor(____callee);					\
		(void*)____glob;					\
	})

#endif

#endif	/* INCLUDED_coru_h_ */
