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
#      include <stdbool.h>

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
#if NEED_VALUE_MUTEX
    /* If the compiler doesn't support atomic intrinsics, the backup plan is
     * a mutex. */
    int paired;
    sim_mutex_t *value_lock;
#endif

    SIM_ATOMIC_TYPE(sim_atomic_type_t) value;
} sim_atomic_value_t;

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Initialization, destruction:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

static SIM_INLINE void sim_atomic_init(sim_atomic_value_t *p)
{
#if NEED_VALUE_MUTEX
    p->paired = 0;
    p->value_lock = (sim_mutex_t *) malloc(sizeof(sim_mutex_t));
    sim_mutex_init(p->value_lock);
#endif

    p->value = 0;
}

static SIM_INLINE void sim_atomic_paired_init(sim_atomic_value_t *p, sim_mutex_t *mutex)
{
#if NEED_VALUE_MUTEX
    p->paired = 1;
    p->value_lock = mutex;
#else
    (void) mutex;
#endif

p->value = 0;
}

static SIM_INLINE void sim_atomic_destroy(sim_atomic_value_t *p)
{
#if NEED_VALUE_MUTEX
    if (!p->paired) {
        sim_mutex_destroy(p->value_lock);
        free(p->value_lock);
    }

    p->paired = 0;
    p->value_lock = NULL;
#endif
    p->value = -1;
}

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Primitives:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

static SIM_INLINE sim_atomic_type_t sim_atomic_get(const sim_atomic_value_t *p)
{
    sim_atomic_type_t retval;

#if HAVE_STD_ATOMIC
    retval = p->value;
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
    p->value = newval;
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
    retval = (p->value += x);
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
    /* Returns the old p->value value. */
    retval = (p->value -= x);
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
    retval = ++p->value;
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
    retval = --p->value;
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


/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * sim_tailq_t: Lock-free (mostly) tail queue, where inserting elements at the head of the queue and appending elements
 * at the tail are atomically compared/exchanged.
 *
 * Lock-free (mostly): As with atomic variables above, use (in order of precedence and availability):
 *  -- C11 and newer standard atomic operations
 *  -- Compiler intrinsics (GCC, Clang and MSVC)
 *  -- Mutex guard as a last resort
 *
 * The queue's tail always points to the next insertion point, so appending to the queue is O(1).
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

/* List element type: */
typedef struct sim_tailq_elem_s {
    /* Generic element pointer. */
    void *elem;
    /* Next element in the tail queue. */
    SIM_ATOMIC_TYPE(struct sim_tailq_elem_s *) next;
} sim_tailq_elem_t;

typedef SIM_ATOMIC_TYPE(sim_tailq_elem_t *) sim_tailq_head_t;
typedef SIM_ATOMIC_TYPE(sim_tailq_head_t *) sim_tailq_tail_t;

/* The list type, itself. */
typedef struct sim_tailq_s {
    /* List head, first element. */
    sim_tailq_head_t head;
    /* List tail, next insertion point. */
    sim_tailq_tail_t tail;
    /* Element counter. */
    sim_atomic_value_t n_elements;

#if NEED_VALUE_MUTEX
    /* If the compiler doesn't support atomic intrinsics, the backup plan is
     * a mutex. */
    int paired;
    sim_mutex_t *lock;
#endif
} sim_tailq_t;

/* Initialize a tail queue. */
void sim_tailq_init(sim_tailq_t *p);
/* Initialize a tail queue, paired with a mutex. */
void sim_tailq_paired_init(sim_tailq_t *p, sim_mutex_t *mutex);
/* Clean up the queue and deallocate the queue's sim_tailq_elem_t elements.
 *
 * Optionally, if free_elems != 0, free the sim_tailq_elem_t's elem as well.
 */
void sim_tailq_destroy(sim_tailq_t *p, int free_elems);

/* Insert at the head of the tail queue. */
sim_tailq_t *sim_tailq_insert_head(sim_tailq_t *p, void *elem);
/* Append to the tail of the tail queue. Returns the */
sim_tailq_t *sim_tailq_append(sim_tailq_t *p, void *elem);
/* Take the tail queue from a source queue, placing it in the destination queue. The
 * source queue is reinitialized to empty. */
sim_tailq_t *sim_tailq_take(sim_tailq_t *src, sim_tailq_t *dst);
/* Splice one tail queue onto the end of another. The 'onto' tail queue has the
 * 'onto' and 'from' combined contents. The 'from' tail queue is emptied. */
sim_tailq_t *sim_tailq_splice(sim_tailq_t *onto, sim_tailq_t *from);
/* Take and dequeue the head of the list, returning the element. */
void *sim_tailq_dequeue_head(sim_tailq_t *p);


/*!
 * Element acccessor function.
 *
 * \param node A tail queue node
 * \return node->elem
 */
static SIM_INLINE void *sim_tailq_element(const sim_tailq_elem_t *node)
{
    return node->elem;
}

/*!
 * Iterator pointer to the tail queue's head. The access to the head element node is atomic.
 *
 * \param p The tail queue
 * \return A pointer to the first tail queue list element.
 */
static SIM_INLINE sim_tailq_elem_t *sim_tailq_iter_head(const sim_tailq_t *p)
{
#if HAVE_STD_ATOMIC
    return atomic_load(&p->head);
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQUIRE) && (defined(__GNUC__) || defined(__clang__))
    sim_tailq_elem_t *retval;
    __atomic_load(&p->head, &retval, __ATOMIC_ACQUIRE);
    return retval;
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(_M_IX86) || defined(_M_X64)
        /* Intel Total Store Ordering optimization. */
        return p->head;
#    else
        return InterlockedExchangePointer(&p->head, p->head);
#    endif
#  else
#    error "sim_atomic_get: No intrinsic?"
#  endif
#else
    sim_tailq_elem_t *retval;

    sim_mutex_lock(p->lock);
    retval = p->head;
    sim_mutex_unlock(p->lock);

    return retval;
#endif
}

/* Iterator pointer to the next element. */
static SIM_INLINE sim_tailq_elem_t *sim_tailq_iter_next(const sim_tailq_elem_t *p)
{
#if HAVE_STD_ATOMIC
    return p->next;
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQUIRE) && (defined(__GNUC__) || defined(__clang__))
    sim_tailq_elem_t *retval;
    __atomic_load(&p->next, &retval, __ATOMIC_ACQUIRE);
    return retval;
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(_M_IX86) || defined(_M_X64)
    /* Intel Total Store Ordering optimization. */
    return p->next;
#    else
    return InterlockedExchangePointer(&p->next, p->next);
#    endif
#  else
#    error "sim_atomic_get: No intrinsic?"
#  endif
#else /* NEED_VALUE_LOCK */
    return p->next;
#endif
}

/* Get the current element count: */
static SIM_INLINE sim_atomic_type_t sim_tailq_count(const sim_tailq_t *p)
{
    return sim_atomic_get(&p->n_elements);
}

/*!
 * Test for empty tail queue.
 *
 * \return 0 if not empty, otherwise 1.
 */
static SIM_INLINE int sim_tailq_empty(const sim_tailq_t * const p)
{
    return (p->head == NULL);
}

#define SIM_ATOMIC_H
#endif
