/* Atomic get/put/add/sub/inc/dec support. Fashioned after the SDL2 approach to
 * atomic variables. Uses compiler and platform intrinsics whenever possible,
 * falling back to a mutex-based approach when necessary.
 *
 * Supported compilers: GCC and Clang across all platforms (including Windows,
 *   if compiling within a MinGW environment.)
 *
 * Platforms: Windows with MSVC and MinGW environments.
 *
 * Types:
 *
 *   sim_atomic_value_t: The wrapper structure that encapsulates the atomic
 *     value. Normally, if using GCC, Clang or MSVC, the value is managed via the
 *     appropriate intrinsics. The fallback is a pthread mutex.
 *
 *   sim_atomic_type_t: The underlying type inside the sim_atomic_value_t wrapper.
 *     This is a long for GCC and Clang, LONG for Windows.
 *
 * sim_atomic_init(sim_atomic_value_t *): Initialize an atomic wrapper. The
 *   value is set to 0 and the mutex is initialized (when necessary.)
 *
 * sim_atomic_paired_init(sim_atomic_value_t *, sim_mutex_t *): Initialize
 *   an atomic wrapper with a provided mutex, to allow mutex sharing when the
 *   mutex is actually needed. The mutex should be initialized as a recursive
 *   mutex.
 *
 *   Use case: Multiple atomic variables are protected by a common mutex. Lock
 *   the mutex before operating on the atomic variables, unlocking when finished.
 *   For example:
 *
 *     pthread_mutex_t common_mutex;
 *     pthread_mutexattr_t recursive;
 *     sim_atomic_type_t var1, var2;
 *
 *     pthread_mutexattr_init (&recursive);
 *     pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE);
 *     pthread_mutex_init(&common_mutex, &recursive);
 *     sim_atomic_paired_init(&var1, &common_mutex);
 *     sim_atomic_paired_init(&var2, &common_mutex);
 *     ...
 *     pthread_mutex_lock(&common_mutex);
 *     sim_atomic_add(&var1, 2);
 *     sim_atomic_add(&var2, 3);
 *     pthread_mutex_unlock(&common_mutex);
 *
 * sim_atomic_destroy(sim_atomic_value_t *p): The inverse of sim_atomic_init().
 *   The value is set to -1. When necessary, the mutex is destroyed.
 *
 * sim_atomic_get(sim_atomic_value_t *p): Atomically returns the
 * sim_atomic_type_t value in the wrapper.
 *
 * sim_atomic_put(sim_atomic_value_t *p, sim_atomic_type_t newval): Atomically
 * stores a new value in the wrapper.
 *
 * sim_atomic_add, sim_atomic_sub(sim_atomic_value_t *p, sim_atomic_type_t x):
 *   Atomically add or subtract a quantity to or from the wrapper's value.
 *
 * sim_atomic_inc, sim_atomic_dec(sim_atomic_value_t *p): Atomically increment or
 *   decrement the wrapper's value, returning the incremented or decremented value.
 */

#include "sim_threads.h"

#if !defined(SIM_ATOMIC_H)

#if !defined(SIM_ATOMIC_MUTEX_ONLY)
#  if !defined(__STDC_NO_ATOMICS__) && __STDC_VERSION__ >= 201112L
   /* C11 or newer compiler -- use the compiler's support for atomic types. */
#    include <stdatomic.h>
#    define HAVE_STD_ATOMIC 1
#    define SIM_ATOMIC_TYPE(X) _Atomic(X)
#  else
/* TODO:  defined(__DECC_VER) && defined(_IA64) -- DEC C on Itanium looks like it
 * might be the same as Windows' interlocked API. Contribtion/correction needed. */

/* NON-FEATURE: Older GCC __sync_* primitives. These have been deprecated for a long
 * time. Explicitly not supported. */

#    define HAVE_STD_ATOMIC 0
#    define SIM_ATOMIC_TYPE(X) X volatile

#    if (defined(_WIN32) || defined(_WIN64)) || \
         (defined(__ATOMIC_ACQ_REL) && defined(__ATOMIC_SEQ_CST) && defined(__ATOMIC_ACQUIRE) && \
          defined(__ATOMIC_RELEASE))
       /* Atomic operations available! */
#      define HAVE_ATOMIC_PRIMS 1
#    else
#      define HAVE_ATOMIC_PRIMS 0
#    endif
#  endif
#endif

#if (defined(_WIN32) || defined(_WIN64))
#  define WINDOWS_LEAN_AND_MEAN
#  include <windows.h>
   typedef LONG sim_atomic_type_t;
#  define PRIsim_atomic "ld"
#else
   typedef long sim_atomic_type_t;
#  define PRIsim_atomic "ld"
#endif

/* NEED_VALUE_MUTEX: Shorthand for where we need to manipulate or maintain the
 * the "atomic" value's mutex. */
#if !defined(SIM_ATOMIC_MUTEX_ONLY) && (HAVE_ATOMIC_PRIMS || HAVE_STD_ATOMIC)
#  define NEED_VALUE_MUTEX 0
#else
#  define NEED_VALUE_MUTEX 1
#  define SIM_ATOMIC_TYPE(X) X
#endif

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Value type and wrapper for integral (numeric) atomics:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

typedef struct {
    SIM_ATOMIC_TYPE(sim_atomic_type_t) value;

#if NEED_VALUE_MUTEX
    /* If the compiler doesn't support atomic intrinsics, the backup plan is
     * a mutex. */
    int paired;
    sim_mutex_t *value_lock;
#endif
} sim_atomic_value_t;

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Initialization, destruction:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

static SIM_INLINE void sim_atomic_init(sim_atomic_value_t *p)
{
    p->value = 0;

#if NEED_VALUE_MUTEX
    p->paired = 0;
    p->value_lock = (sim_mutex_t *) malloc(sizeof(sim_mutex_t));
    sim_mutex_init(p->value_lock);
#endif
}

static SIM_INLINE void sim_atomic_paired_init(sim_atomic_value_t *p, sim_mutex_t *mutex)
{
p->value = 0;

#if NEED_VALUE_MUTEX
    p->paired = 1;
    p->value_lock = mutex;
#else
    (void) mutex;
#endif
}

static SIM_INLINE void sim_atomic_destroy(sim_atomic_value_t *p)
{
    p->value = -1;

    #if NEED_VALUE_MUTEX
    if (!p->paired) {
        sim_mutex_destroy(p->value_lock);
        free(p->value_lock);
    }

    p->paired = 0;
    p->value_lock = NULL;
#endif
}

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Primitives:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

static SIM_INLINE sim_atomic_type_t sim_atomic_get(const sim_atomic_value_t *p)
{
    sim_atomic_type_t retval;

#if HAVE_STD_ATOMIC
    retval = atomic_load(&p->value);
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQUIRE) && (defined(__GNUC__) || defined(__clang__))
        __atomic_load(&p->value, &retval, __ATOMIC_ACQUIRE);
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(_M_IX86) || defined(_M_X64)
        /* Intel Total Store Ordering optimization. */
        retval = p->value;
#    else
        retval = InterlockedOr(&p->value, 0);
#    endif
#  else
#    error "sim_atomic_get: No intrinsic?"
        retval = -1;
#  endif
#else
    sim_mutex_lock(p->value_lock);
    retval = p->value;
    sim_mutex_unlock(p->value_lock);
#endif

    return retval;
}

static SIM_INLINE void sim_atomic_put(sim_atomic_value_t *p, sim_atomic_type_t newval)
{
#if HAVE_STD_ATOMIC
    atomic_store(&p->value, newval);
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_RELEASE) && (defined(__GNUC__) || defined(__clang__))
    __atomic_store(&p->value, &newval, __ATOMIC_RELEASE);
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(_M_IX86) || defined(_M_X64)
        /* Intel Total Store Ordering optimization. */
        p->value = newval;
#    else
        InterlockedExchange(&p->value, newval);
#    endif
#  else
#    error "sim_atomic_put: No intrinsic? Need __ATOMIC_RELEASE"
#  endif
#else
    sim_mutex_lock(p->value_lock);
    p->value = newval;
    sim_mutex_unlock(p->value_lock);
#endif
}

static SIM_INLINE sim_atomic_type_t sim_atomic_add(sim_atomic_value_t *p, sim_atomic_type_t x)
{
    sim_atomic_type_t retval;

#if HAVE_STD_ATOMIC
    /* atomic_fetch_add returns the old p->value value. */
    retval = atomic_fetch_add(&p->value, x) + x;
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQ_REL)
#    if (defined(__GNUC__) || defined(__clang__))
    retval = __atomic_add_fetch(&p->value, x, __ATOMIC_ACQ_REL);
#    else
#      error "sim_atomic_add: No atomic add intrinsic?"
#    endif
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(InterlockedAdd)
        retval = InterlockedAdd(&p->value, x);
#    else
        /* Older Windows InterlockedExchangeAdd, which returns the original value in p->value. */
        InterlockedExchangeAdd(&p->value, x);
        retval = p->value;
#    endif
#  else
#    error "sim_atomic_add: No intrinsic?"
#  endif
#else
    sim_mutex_lock(p->value_lock);
    retval = (p->value += x);
    sim_mutex_unlock(p->value_lock);
#endif

    return retval;
}

static SIM_INLINE sim_atomic_type_t sim_atomic_sub(sim_atomic_value_t *p, sim_atomic_type_t x)
{
    sim_atomic_type_t retval;

#if HAVE_STD_ATOMIC
    /* atomic_fetch_sub returns the old p->value value. */
    retval = atomic_fetch_sub(&p->value, x) - x;
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQ_REL)
#    if defined(__GNUC__) || defined(__clang__)
        retval = __atomic_sub_fetch(&p->value, x, __ATOMIC_ACQ_REL);
#    else
#      error "sim_atomic_sub: No atomic sub intrinsic?"
#    endif
#  elif defined(_WIN32) || defined(_WIN64)
    /* There isn't a InterlockedSub function. Revert to basic math(s). */
#    if defined(InterlockedAdd)
        retval = InterlockedAdd(&p->value, -x);
#    else
        /* Older Windows InterlockedExchangeAdd, which returns the original value in p->value. */
        InterlockedExchangeAdd(&p->value, -x);
        retval = p->value;
#    endif
#  else
#    error "sim_atomic_sub: No intrinsic?"
#  endif
#else
    sim_mutex_lock(p->value_lock);
    retval = (p->value -= x);
    sim_mutex_unlock(p->value_lock);
#endif

    return retval;
}

static SIM_INLINE sim_atomic_type_t sim_atomic_inc(sim_atomic_value_t *p)
{
    sim_atomic_type_t retval;

#if HAVE_STD_ATOMIC
    retval = sim_atomic_add(p, 1);
#elif HAVE_ATOMIC_PRIMS
#  if !defined(_WIN32) && !defined(_WIN64)
        retval = sim_atomic_add(p, 1);
#  elif defined(_WIN32) || defined(_WIN64)
        retval = InterlockedIncrement(&p->value);
#  else
#    error "sim_atomic_inc: No intrinsic?"
#  endif
#else
    sim_mutex_lock(p->value_lock);
    retval = ++p->value;
    sim_mutex_unlock(p->value_lock);
#endif

    return retval;
}

static SIM_INLINE sim_atomic_type_t sim_atomic_dec(sim_atomic_value_t *p)
{
    sim_atomic_type_t retval;

#if HAVE_STD_ATOMIC
       retval = sim_atomic_sub(p, 1);
#elif HAVE_ATOMIC_PRIMS
#  if !defined(_WIN32) && !defined(_WIN64)
        retval = sim_atomic_sub(p, 1);
#  elif defined(_WIN32) || defined(_WIN64)
        retval = InterlockedDecrement(&p->value);
#  else
#    error "sim_atomic_dec: No intrinsic?"
#  endif
#else
    sim_mutex_lock(p->value_lock);
    retval = --p->value;
    sim_mutex_unlock(p->value_lock);
#endif

    return retval;
}

#define SIM_ATOMIC_H
#endif
