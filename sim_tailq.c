
/* Definitions needed when compiling the unit tests to avoid sim_defs.h overhead: */

#if !defined(SIM_INLINE)
#  if defined(_MSC_VER)
#    define SIM_INLINE _inline
#  elif defined(__GNUC__) || defined(__clang__)
#    define SIM_INLINE inline
#  else
#    define SIM_INLINE
#  endif
#endif

#include <stdlib.h>
#include "support/sim_bool.h"
#include "sim_tailq.h"

/* Forward decl's: */
static SIM_INLINE sim_tailq_elem_t *tailq_alloc(sim_tailq_t *tailq);
static SIM_INLINE sim_tailq_elem_t *advance_head(sim_tailq_t *tailq);
static SIM_INLINE sim_tailq_elem_t *advance_tail(sim_tailq_t *tailq);
static SIM_INLINE sim_tailq_elem_t *tailq_add_node(sim_tailq_t *tailq);

static SIM_INLINE void tailq_lock(sim_tailq_t *tailq);
static SIM_INLINE void tailq_unlock(sim_tailq_t *tailq);

static SIM_INLINE void set_item_status(SIM_ATOMIC_TYPE(sim_tailq_elem_t *) elem, const sim_tailq_t *tailq,
sim_tailq_item_status_t status);

static const size_t INITIAL_TAILQ_NODES = 17;

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Initialization, destruction:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

int sim_tailq_init(sim_tailq_t* tailq)
{
    if ((tailq->head = tailq->tail = tailq_alloc(tailq)) == NULL)
        return 0;
    
#if NEED_VALUE_MUTEX
    tailq->lock = (sim_mutex_t*)malloc(sizeof(sim_mutex_t));
    sim_mutex_recursive(tailq->lock);
    tailq->paired = FALSE;

    /* This mutex is paired with the element counter. */
    sim_atomic_paired_init(&tailq->n_elements, tailq->lock);
#endif

    sim_atomic_put(&tailq->n_elements, 0);
    return 1;
}

int sim_tailq_paired_init(sim_tailq_t* tailq, sim_mutex_t* mutex)
{
    if ((tailq->head = tailq->tail = tailq_alloc(tailq)) == NULL)
        return 0;

#if NEED_VALUE_MUTEX
    tailq->paired = TRUE;
    tailq->lock = mutex;
    sim_atomic_paired_init(&tailq->n_elements, tailq->lock);
#endif

    sim_atomic_put(&tailq->n_elements, 0);
    return 1;
}

void sim_tailq_destroy(sim_tailq_t* tailq, int free_elems)
{
    tailq_lock(tailq);

    sim_tailq_elem_t* p;

    for (p = sim_tailq_head(tailq); sim_tailq_next(p, tailq) != sim_tailq_head(tailq); /* empty */) {
        sim_tailq_elem_t* p_next = get_tailq_pointer(&p->next, tailq);

        if (free_elems)
            free(p->item);
        free(p);
        p = p_next;
    }

    if (free_elems)
        free(p->item);
    free(p);

    tailq->head = tailq->tail = NULL;
    sim_atomic_destroy(&tailq->n_elements);
    tailq_unlock(tailq);

#if NEED_VALUE_MUTEX
    if (!tailq->paired) {
        free(tailq->lock);
    }
#endif
}

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Basic operations:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

sim_tailq_t *sim_tailq_enqueue_xform(sim_tailq_t* tailq, sim_tailq_xform_t xform, void* xform_arg)
{
    if (get_tailq_pointer(&tailq->tail->next, tailq) == sim_tailq_head(tailq)) {
        tailq_add_node(tailq);
    }

    sim_tailq_elem_t* exist_elem = advance_tail(tailq);

    set_item_status(exist_elem, tailq, TAILQ_ITEM_BUSY);
    exist_elem->item = xform(exist_elem->item, xform_arg);
    set_item_status(exist_elem, tailq, TAILQ_ITEM_READY);

    return tailq;
}

sim_tailq_item_t sim_tailq_dequeue(sim_tailq_t* tailq)
{
    sim_tailq_item_t item = NULL;

    if (sim_tailq_head(tailq) != sim_tailq_tail(tailq)) {
        sim_tailq_elem_t *elem = advance_head(tailq);

        /* Spin until ready. */
        while (sim_tailq_item_status(elem, tailq) != TAILQ_ITEM_READY) {
            /* Spin. */
        }

        item = elem->item;
    }

    return item;
}

static SIM_INLINE sim_tailq_item_t identity_item_transform(sim_tailq_item_t ignored, void* new_item)
{
    (void) ignored;
    return new_item;
}

sim_tailq_t* sim_tailq_enqueue(sim_tailq_t* tailq, sim_tailq_item_t elem)
{
    return sim_tailq_enqueue_xform(tailq, identity_item_transform, elem);
}

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Utilities:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

sim_tailq_elem_t* tailq_alloc(sim_tailq_t* tailq)
{
    size_t i;
    sim_tailq_elem_t *first = NULL, *last;

    for (i = 0; i < INITIAL_TAILQ_NODES; ++i) {
        sim_tailq_elem_t* p = (sim_tailq_elem_t*) malloc(sizeof(sim_tailq_elem_t));

        if (p != NULL) {
            p->item = NULL;
            p->item_status = TAILQ_ITEM_READY;

            if (first != NULL) {
                last->next = p;
                last = p;
                last->next = first;
            } else {
                first = last = p;
                p->next = p;
            }
        } else {
            return NULL;
        }
    }

    sim_atomic_put(&tailq->n_allocated, INITIAL_TAILQ_NODES);
    return first;
}

sim_tailq_elem_t* advance_head(sim_tailq_t* tailq)
{
    sim_tailq_elem_t* current_head;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    bool did_xchg;

    do {
        current_head = sim_tailq_head(tailq);
#  if HAVE_STD_ATOMIC
        did_xchg = atomic_compare_exchange_strong(&tailq->head, &current_head, tailq->head->next);
#  elif HAVE_ATOMIC_PRIMS
#    if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
        did_xchg = __atomic_compare_exchange(&tailq->head, &current_head, &tailq->head->next, false, __ATOMIC_SEQ_CST,
                                             __ATOMIC_SEQ_CST);
#    elif defined(_WIN32) || defined(_WIN64)
        PVOID retval = InterlockedCompareExchangePointer(&tailq->head, tailq->head->next, current_head);
        did_xchg = (retval == current_head);
#    endif
#  endif
    } while (!did_xchg);
#else
    sim_mutex_lock(tailq->lock);
    current_head = tailq->head;
    tailq->head = tailq->head->next;
    sim_mutex_unlock(tailq->lock);
#endif

    sim_atomic_dec(&tailq->n_elements);
    return current_head;
}

sim_tailq_elem_t* advance_tail(sim_tailq_t* tailq)
{
    sim_tailq_elem_t* current_tail;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    bool did_xchg;

    do {
        current_tail = sim_tailq_tail(tailq);
#  if HAVE_STD_ATOMIC
        did_xchg = atomic_compare_exchange_strong(&tailq->tail, &current_tail, current_tail->next);
#  elif HAVE_ATOMIC_PRIMS
#    if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
        did_xchg = __atomic_compare_exchange(&tailq->tail, &current_tail, &current_tail->next, false, __ATOMIC_SEQ_CST,
                                             __ATOMIC_SEQ_CST);
#    elif defined(_WIN32) || defined(_WIN64)
        PVOID retval = InterlockedCompareExchangePointer(&tailq->tail, current_tail->next, current_tail);
        did_xchg = (retval == current_tail);
#    endif
#  endif
    } while (!did_xchg);
#else
    sim_mutex_lock(tailq->lock);
    current_tail = tailq->tail;
    tailq->tail = tailq->tail->next;
    sim_mutex_unlock(tailq->lock);
#endif

    sim_atomic_inc(&tailq->n_elements);
    return current_tail;
}

sim_tailq_elem_t* tailq_add_node(sim_tailq_t* tailq)
{
    sim_tailq_elem_t* current_next;
    sim_tailq_elem_t* node = (sim_tailq_elem_t*)malloc(sizeof(sim_tailq_elem_t));

    if (node == NULL)
        return NULL;

    node->item = NULL;
    node->item_status = TAILQ_ITEM_READY;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    bool did_xchg;

    do {
        current_next = get_tailq_pointer(&tailq->tail->next, tailq);
        node->next = current_next;

#  if HAVE_STD_ATOMIC
        did_xchg = atomic_compare_exchange_strong(&tailq->tail->next, &current_next, node);
#  elif HAVE_ATOMIC_PRIMS
#    if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
        did_xchg =
            __atomic_compare_exchange(&tailq->tail->next, &current_next, &node, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#    elif HAVE_ATOMIC_PRIMS && (defined(_WIN32) || defined(_WIN64))
        PVOID retval = InterlockedCompareExchangePointer(&tailq->tail->next, node, current_next);
        did_xchg = (retval == current_next);
#    endif
#  endif
    } while (!did_xchg);
#else
    sim_mutex_lock(tailq->lock);
    node->next = tailq->tail->next;
    tailq->tail->next = node;
    sim_mutex_unlock(tailq->lock);
#endif

    sim_atomic_inc(&tailq->n_allocated);
    return node;
}

void tailq_lock(sim_tailq_t* tailq)
{
#if NEED_VALUE_MUTEX
    sim_mutex_lock(tailq->lock);
#endif
}

void tailq_unlock(sim_tailq_t* tailq)
{
#if NEED_VALUE_MUTEX
    sim_mutex_unlock(tailq->lock);
#endif
}

static SIM_INLINE void set_item_status(SIM_ATOMIC_TYPE(sim_tailq_elem_t*) elem, const sim_tailq_t* tailq,
                                       sim_tailq_item_status_t status)
{
#if HAVE_STD_ATOMIC
    (void)tailq;
    atomic_store(&elem->item_status, status);
#elif HAVE_ATOMIC_PRIMS
#  if defined(__ATOMIC_ACQUIRE) && (defined(__GNUC__) || defined(__clang__))
    (void)tailq;
    __atomic_store(&elem->item_status, &status, __ATOMIC_RELEASE);
#  elif defined(_WIN32) || defined(_WIN64)
#    if defined(_M_IX86) || defined(_M_X64)
    /* Intel Total Store Ordering optimization. */
    elem->item_status = status;
    (void)tailq;
#    else
    InterlockedExchange(&elem->item_status, status);
    (void)tailq;
#    endif
#  else
#error "set_item_status: No intrinsic?"
    retval = -1;
#  endif
#else
    sim_mutex_lock(tailq->lock);
    elem->item_status = status;
    sim_mutex_unlock(tailq->lock);
#endif
}
