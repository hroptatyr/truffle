#ifndef __CORO_H__
#define __CORO_H__

/*
 * Portable coroutines for C. Caveats:
 *
 * 1. You should not take the address of a stack variable, since stack management
 *    could reallocate the stack, the new stack would reference a variable in the
 *    old stack. Also, cloning a coroutine would cause the cloned coroutine to
 *    reference a variable in the other stack.
 * 2. You must call coro_init for each kernel thread, since there are thread-local
 *    data structures. This will eventually be exploited to scale coroutines across
 *    CPUs.
 * 3. If setjmp/longjmp inspect the jmp_buf structure before executing a jump, this
 *    library probably will not work.
 *
 * Refs:
 * http://www.yl.is.s.u-tokyo.ac.jp/sthreads/
 */

#include <stdlib.h>
#include <stdint.h>

/* a coroutine handle */
typedef struct _coro *coro;

typedef intptr_t cvalue;

/* the type of entry function */
typedef cvalue (*_entry)(cvalue init, cvalue inargs);

/*
 * Initialize the coroutine library, returning a coroutine for the thread that called init.
 */
extern coro coro_init(void);

/*
 * Create a new coroutine from the given function, and with the
 * given stack.
 */
extern coro coro_new(_entry fn, cvalue init);

/*
 * Invoke a coroutine passing the given value.
 */
extern cvalue coro_call(coro target, cvalue value);

/*
 * Return to calling coroutine and pass on VALUE.
 */
extern cvalue coro_yield(cvalue value);

/*
 * Clone a given coroutine. This can be used to implement multishot continuations.
 */
coro coro_clone(coro c);

/*
 * Free the coroutine and return the space for the stack.
 */
extern void coro_free(coro c);

/*
 * Poll the current coroutine to ensure sufficient resources are allocated. This
 * should be called periodically to ensure a coroutine doesn't segfault.
 */
extern void coro_poll(void);

#endif /* __CORO_H__ */
