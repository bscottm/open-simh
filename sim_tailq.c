
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
#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
#endif

static SIM_INLINE sim_tailq_elem_t *tailq_alloc();
static SIM_INLINE sim_tailq_elem_t *advance_head(sim_tailq_t *tailq);
static SIM_INLINE sim_tailq_elem_t *advance_tail(sim_tailq_t *tailq);
static SIM_INLINE sim_tailq_elem_t *tailq_add_node(sim_tailq_t *tailq);

static SIM_INLINE void tailq_lock(sim_tailq_t *tailq);
static SIM_INLINE void tailq_unlock(sim_tailq_t *tailq);

static const size_t INITIAL_TAILQ_NODES = 17;

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Initialization, destruction:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

int sim_tailq_init(sim_tailq_t *tailq)
{
    /* This mutex is paired with the element counter. */
    tailq->paired = FALSE;
    if ((tailq->head = tailq->tail = tailq_alloc()) == NULL)
        return 0;

#if NEED_VALUE_MUTEX
    tailq->lock = (sim_mutex_t *) malloc(sizeof(sim_mutex_t));
    sim_mutex_recursive(tailq->lock);
    sim_atomic_paired_init(&tailq->n_elements, tailq->lock);
#endif

    sim_atomic_put(&tailq->n_elements, 0);
    return 1;
}

int sim_tailq_paired_init(sim_tailq_t *tailq, sim_mutex_t *mutex)
{
    tailq->paired = TRUE;
    if ((tailq->head = tailq->tail = tailq_alloc()) == NULL)
        return 0;

#if NEED_VALUE_MUTEX
    tailq->lock = mutex;
    sim_atomic_paired_init(&tailq->n_elements, tailq->lock);
#endif

    sim_atomic_put(&tailq->n_elements, 0);
    return 1;
}

void sim_tailq_destroy(sim_tailq_t *tailq, int free_elems)
{
    tailq_lock(tailq);

    sim_tailq_elem_t *p;
    
    for (p = tailq->head; get_tailq_pointer(&p->next) != get_tailq_pointer(&tailq->head); /* empty */) {
        sim_tailq_elem_t *p_next = get_tailq_pointer(&p->next);

        if (free_elems)
            free(p->elem);
        free(p);
        p = p_next;
    }

    if (free_elems)
        free(p->elem);
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

sim_tailq_t *sim_tailq_enqueue(sim_tailq_t *tailq, void *elem)
{
    if (get_tailq_pointer(&tailq->tail->next) == sim_tailq_head(tailq)) {
        tailq_add_node(tailq);
    }

    advance_tail(tailq)->elem = elem;
    return tailq;
}

void *sim_tailq_dequeue(sim_tailq_t *tailq)
{
    return (sim_tailq_head(tailq) != sim_tailq_tail(tailq) ? advance_head(tailq)->elem : NULL);
}


/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Utilities:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

sim_tailq_elem_t *tailq_alloc()
{
    size_t i;
    sim_tailq_elem_t *first = NULL, *last;

    for (i = 0; i < INITIAL_TAILQ_NODES; ++i) {
        sim_tailq_elem_t *p = (sim_tailq_elem_t *) malloc(sizeof(sim_tailq_elem_t));

        if (p != NULL) {
            p->elem = NULL;

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

    return first;
}

sim_tailq_elem_t *advance_head(sim_tailq_t *tailq)
{
    sim_tailq_elem_t *current_head;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    bool did_xchg;
    
    do {
        current_head = tailq->head;
#  if HAVE_STD_ATOMIC
        did_xchg = atomic_compare_exchange_strong(&tailq->head, &current_head, tailq->head->next);
#  elif HAVE_ATOMIC_PRIMS 
#    if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
        did_xchg = __atomic_compare_exchange(&tailq->head, &current_head, &tailq->head->next,
                                             false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
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

sim_tailq_elem_t *advance_tail(sim_tailq_t *tailq)
{
    sim_tailq_elem_t *current_tail;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    bool did_xchg;
    
    do {
        current_tail = sim_tailq_tail(tailq);
#  if HAVE_STD_ATOMIC
        did_xchg = atomic_compare_exchange_strong(&tailq->tail, &current_tail, current_tail->next);
#  elif HAVE_ATOMIC_PRIMS
#    if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
        did_xchg = __atomic_compare_exchange(&tailq->tail, &current_tail, &current_tail->next, 
                                             false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#    elif HAVE_ATOMIC_PRIMS && (defined(_WIN32) || defined(_WIN64))
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

sim_tailq_elem_t *tailq_add_node(sim_tailq_t *tailq)
{
    sim_tailq_elem_t *current_next;
    sim_tailq_elem_t *node = (sim_tailq_elem_t *) malloc(sizeof(sim_tailq_elem_t));

    if (node == NULL)
        return NULL;

     node->elem = NULL;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    bool did_xchg;
    
    do {
        current_next = get_tailq_pointer(&tailq->tail->next);
        node->next = current_next;

#  if HAVE_STD_ATOMIC
        did_xchg = atomic_compare_exchange_strong(&tailq->tail->next, &current_next, node);
#  elif HAVE_ATOMIC_PRIMS
#    if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
        did_xchg = __atomic_compare_exchange(&tailq->tail->next, &current_next, &node, 
                                             false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
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

    return node;
}

void tailq_lock(sim_tailq_t *tailq)
{
#if NEED_VALUE_MUTEX
    sim_mutex_lock(tailq->lock);
#endif
}

void tailq_unlock(sim_tailq_t *tailq)
{
#if NEED_VALUE_MUTEX
    sim_mutex_unlock(tailq->lock);
#endif
}
