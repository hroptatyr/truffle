#ifndef __CTXT_H__
#define __CTXT_H__

#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>

/* context management definitions */
typedef jmp_buf _ctxt;

#ifndef _setjmp
#define _setjmp setjmp
#endif

#ifndef _longjmp
#define _longjmp longjmp
#endif

#define _save_and_resumed(c) _setjmp(c)
#define _rstr_and_jmp(c) _longjmp(c, 1)

#define SP_ALIGN(sp) sp
/*#define SP_ALIGN(sp) (sp + sp % 16)*/

/* the list of offsets in jmp_buf to be adjusted */
/* # of offsets cannot be greater than jmp_buf */
static ptrdiff_t _offsets[sizeof(jmp_buf) / sizeof(int)];
static size_t _offsets_len;

/* true if stack grows up, false if down */
static bool _stack_grows_up;

/* the offset of the beginning of the stack frame in a function */
static ptrdiff_t _frame_offset;

/* This probing code is derived from Douglas Jones' user thread library */
struct _probe_data {
	intptr_t low_bound;		/* below probe on stack */
	intptr_t probe_local;	/* local to probe on stack */
	intptr_t high_bound;	/* above probe on stack */
	intptr_t prior_local;	/* value of probe_local from earlier call */

	jmp_buf probe_env;	/* saved environment of probe */
	jmp_buf probe_sameAR;	/* second environment saved by same call */
	jmp_buf probe_samePC;	/* environment saved on previous call */

	jmp_buf * ref_probe;	/* switches between probes */
};

static inline void boundhigh(struct _probe_data *p)
{
	int c;
	p->high_bound = (intptr_t)&c;
}

static inline void probe(struct _probe_data *p)
{
	int c;
	p->prior_local = p->probe_local;
	p->probe_local = (intptr_t)&c;
	_setjmp( *(p->ref_probe) );
	p->ref_probe = &p->probe_env;
	_setjmp( p->probe_sameAR );
	boundhigh(p);
}

static inline void boundlow(struct _probe_data *p)
{
	int c;
	p->low_bound = (intptr_t)&c;
	probe(p);
}

static inline void fill(struct _probe_data *p)
{
	boundlow(p);
}

static void _infer_jmpbuf_offsets(struct _probe_data *pb)
{
	/* following line views jump buffer as array of long intptr_t */
	intptr_t * p = (intptr_t *)pb->probe_env;
	intptr_t * sameAR = (intptr_t *)pb->probe_sameAR;
	intptr_t * samePC = (intptr_t *)pb->probe_samePC;
	intptr_t prior_diff = pb->probe_local - pb->prior_local;
	intptr_t min_frame = pb->probe_local;

	for (size_t i = 0; i < sizeof(jmp_buf) / sizeof(intptr_t); ++i) {
		intptr_t pi = p[i], samePCi = samePC[i];
		if (pi != samePCi) {
			if ((pi - samePCi) == prior_diff) {
				/* the i'th pointer field in jmp_buf needs to be save/restored */
				_offsets[_offsets_len++] = i;
				if ((_stack_grows_up && min_frame > pi) || (!_stack_grows_up && min_frame < pi)) {
					min_frame = pi;
				}
			}
		}
	}

	_frame_offset = (_stack_grows_up
		? pb->probe_local - min_frame
		: min_frame - pb->probe_local);
}

static void _infer_direction_from(int *first_addr)
{
	int second;
	_stack_grows_up = (first_addr < &second);
}

static void _infer_stack_direction()
{
	int first;
	_infer_direction_from(&first);
}

static void _probe_arch()
{
	struct _probe_data p;
	p.ref_probe = &p.probe_samePC;

	_infer_stack_direction();

	/* do a probe with filler on stack */
	fill(&p);
	/* do a probe without filler */
	boundlow(&p);
	_infer_jmpbuf_offsets(&p);
}

#endif /*__CTXT_H__*/
