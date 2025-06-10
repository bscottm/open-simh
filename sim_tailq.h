/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * sim_tailq_t: Lock-free (mostly) tail queue, where inserting elements at the head of the queue and appending elements
 * at the tail are atomically compared/exchanged.
 *
 * Lock-free (mostly): In order of precedence and availability:
 *  -- C11 and newer standard atomic operations
 *  -- Compiler intrinsics (GCC, Clang and MSVC)
 *  -- Mutex guard as a last resort
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

#if !defined(SIM_TAILQ_H)

#include "support/sim_bool.h"
#include "sim_atomic.h"

/*! \brief Tail queue item type.
 *
 * Generic pointer to items stored in the tail queue. This forces type casts to
 * the desired pointer type and avoid the potential bugs that can arise from
 * `void *`.
 */
typedef struct sim_tailq_item_s {
    struct sim_tailq_item_s *__dummy_ignored_never_used;
} *sim_tailq_item_t;

/*! \brief Item status indicator.
 *
 * An internal `item` status indicator that ensures that the `sim_tailq_elem_t->item`
 * member is satble. `TAILQ_ITEM_READY` indicates that the `item` pointer is stable,
 * i.e., not being modified. `TAILQ_ITEM_BUSY` indicates that the `item` pointer is
 * not stable, i.e., being modified, and a dequeue operation needs to wait until
 * the status changes to `TAILQ_ITEM_READY`.
 */
typedef enum sim_tailq_item_status_e {
  TAILQ_ITEM_READY,
  TAILQ_ITEM_BUSY
} sim_tailq_item_status_t;

/*! \brief * Tail queue list element type.
 */
typedef struct sim_tailq_elem_s {
    /*! Generic element pointer. */
    sim_tailq_item_t item;
    /*! Item status */
    SIM_ATOMIC_TYPE(sim_tailq_item_status_t) item_status;
    /*! Next element in the tail queue. */
    SIM_ATOMIC_TYPE(struct sim_tailq_elem_s *) next;
} sim_tailq_elem_t;

/* \brief The tailq queue list type.
 *
 * An atomic access controlled queue that also keeps track of the
 * number of elements in the queue and the number of elements
 * allocated.
 * 
 * The queue is initialized using `sim_tailq_init()` or with a
 * paired mutex via `sim_tailq_paired_init()` when mutexes are
 * the sole synchronization primitive.
 * 
 * Items are enqueued and dequeued using `sim_tailq_enqueue()`
 * and `sim_tailq_dequeue()`.
 * 
 * `sim_tailq_destroy()` * cleans the queue when it is no longer
 * needed.
 */
typedef struct sim_tailq_s {
    /*! List head, first element. */
    SIM_ATOMIC_TYPE(sim_tailq_elem_t *) head;
    /*! List tail, next insertion point. */
    SIM_ATOMIC_TYPE(sim_tailq_elem_t *) tail;
    /*! Current active element count. */
    sim_atomic_value_t n_elements;
    /*! Maximum number of elements allocated. */
    sim_atomic_value_t n_allocated;

#if NEED_VALUE_MUTEX
    /*!
     * \brief Modification mutex
     *
     * This mutex is acquired when lock-free or atomic variable support is
     * unavailable, or when modifying the internal allocation state of the tail
     * queue.
     */
    sim_mutex_t *lock;

    /*!
     * \brief Paired mutex flag.
     *
     * This flag indicates whether the mutex is paired with an external mutex,
     * supplied during initialization.
     * 
     * 0: Not paired
     * 1: Paired.
     */
    int paired;
#endif
} sim_tailq_t;

/*! \brief Enqueue transformation function type.
 *
 * `sim_tailq_enqueue_xform()` takes a function whose prototype is
 * `sim_tailq_xform_t` to transform an existing item into a new item.
 * Note that the existing item may be a `NULL` pointer.
 */
typedef sim_tailq_item_t (*sim_tailq_xform_t)(sim_tailq_item_t, void *);

/*! \brief Initialize an atomic tail queue.
 *
 * Bulk allocates an initial tail queue list, makes the list circular,
 * sets the head and tail element pointers.
 * 
 * \param tailq The tail queue to be initialized.
 * \return 0 on error, otherwise 1.
 */
int sim_tailq_init(sim_tailq_t *tailq);

/*! \brief Initialize an atomic tail queue paired with a mutex.
 *
 * 
 * 
 * \param tailq
 * \param mutex 
 */
int sim_tailq_paired_init(sim_tailq_t *tailq, sim_mutex_t *mutex);

/* Clean up the queue and deallocate the queue's sim_tailq_elem_t elements.
 *
 * Optionally, if free_elems != 0, free the sim_tailq_elem_t's item as well.
 */
void sim_tailq_destroy(sim_tailq_t *p, int free_elems);

/* Append to the tail of the tail queue. Returns the */
sim_tailq_t *sim_tailq_enqueue(sim_tailq_t *p, sim_tailq_item_t item);

sim_tailq_t *sim_tailq_enqueue_xform(sim_tailq_t *tailq, sim_tailq_xform_t xform, void *xform_arg);

/* Take and dequeue the head of the list, returning the element. */
sim_tailq_item_t sim_tailq_dequeue(sim_tailq_t *p);

/*!
 * Tail queue element acccessor function.
 *
 * \param node A tail queue node
 * \return node->item
 */
static SIM_INLINE void *sim_tailq_item(const sim_tailq_elem_t *node)
{
    return node->item;
}

static SIM_INLINE sim_tailq_elem_t *get_tailq_pointer(SIM_ATOMIC_TYPE(sim_tailq_elem_t *) const *ptr, const sim_tailq_t *tailq)
{
    sim_tailq_elem_t *retval;

#if HAVE_STD_ATOMIC
    (void) tailq;
    retval = atomic_load(ptr);
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQUIRE) && (defined(__GNUC__) || defined(__clang__))
        (void) tailq;
        __atomic_load(ptr , &retval, __ATOMIC_ACQUIRE);
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(_M_IX86) || defined(_M_X64)
        /* Intel Total Store Ordering optimization. */
        retval = *ptr;
        (void) tailq;
#    else
        retval = InterlockedExchangePointer(ptr, *ptr);
        (void) tailq;
#    endif
#  else
#    error "get_tailq_pointer: No intrinsic?"
        retval = -1;
#  endif
#else
    sim_mutex_lock(tailq->lock);
    retval = *ptr;
    sim_mutex_unlock(tailq->lock);
#endif

    return retval;
}

/*!
 * \brief Tail queue's head node pointer.
 *
 * Returns the tail queue's head pointer, enforcing atomic access. This is only
 * an accessor function. Higher level functions should check for an empty queue
 * before iterating through the tailq queue.
 *
 * \param tailq The tail queue
 * \return A pointer to the first tail queue list element.
 */
static SIM_INLINE sim_tailq_elem_t *sim_tailq_head(const sim_tailq_t *tailq)
{
    return get_tailq_pointer(&tailq->head, tailq);
}

/*!
 * \brief Tail queue's tail node pointer.
 *
 * Tail queue's tail node pointer.enforcing atomic access. This is only an
 * accessor function. Higher level functions should check for an empty queue
 * before iterating through the tailq queue.
 *
 * \param tailq The tail queue
 * \return A pointer to the tail queue tail element.
 */
static SIM_INLINE sim_tailq_elem_t *sim_tailq_tail(const sim_tailq_t *tailq)
{
    return get_tailq_pointer(&tailq->tail, tailq);
}

/* Iterator pointer to the next element. */
static SIM_INLINE sim_tailq_elem_t *sim_tailq_next(const sim_tailq_elem_t *p, const sim_tailq_t *tailq)
{
    return get_tailq_pointer(&p->next, tailq);
}

/*!
 * Iterator pointer to the tail queue's head. The access to the head element node is atomic.
 *
 * \param p The tail queue
 * \return A pointer to the first tail queue list element.
 */
static SIM_INLINE t_bool sim_tailq_at_tail(const sim_tailq_elem_t *p, const sim_tailq_t *tailq)
{
    return (get_tailq_pointer(&tailq->tail, tailq) == p);
}

/* Get the current element count: */
static SIM_INLINE sim_atomic_type_t sim_tailq_count(const sim_tailq_t *tailq)
{
    return sim_atomic_get(&tailq->n_elements);
}

/* Get the total allocation. */
static SIM_INLINE sim_atomic_type_t sim_tailq_allocated(const sim_tailq_t *tailq)
{
    return sim_atomic_get(&tailq->n_allocated);
}

/*!
 * Test for empty tail queue.
 *
 * \return 0 if not empty, otherwise 1.
 */
static SIM_INLINE int sim_tailq_empty(const sim_tailq_t * const tailq)
{
    return (get_tailq_pointer(&tailq->head, tailq) == get_tailq_pointer(&tailq->tail, tailq));
}

static SIM_INLINE sim_tailq_item_status_t sim_tailq_item_status(SIM_ATOMIC_TYPE(sim_tailq_elem_t *) elem, const sim_tailq_t *tailq)
{
    sim_tailq_item_status_t retval;

#if HAVE_STD_ATOMIC
    (void) tailq;
    retval = atomic_load(&elem->item_status);
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQUIRE) && (defined(__GNUC__) || defined(__clang__))
        (void) tailq;
        __atomic_load(&elem->item_status, &retval, __ATOMIC_ACQUIRE);
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(_M_IX86) || defined(_M_X64)
        /* Intel Total Store Ordering optimization. */
        retval = elem->item_status;
        (void) tailq;
#    else
        retval = InterlockedExchange(&elem->item_status, elem->item_status);
        (void) tailq;
#    endif
#  else
#    error "sim_tailq_item_status: No intrinsic?"
        retval = -1;
#  endif
#else
    sim_mutex_lock(tailq->lock);
    retval = elem->item_status;
    sim_mutex_unlock(tailq->lock);
#endif

    return retval;
}

#define SIM_TAILQ_H
#endif
