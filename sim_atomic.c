
/* #define __STDC_NO_ATOMICS__ */

/* Definitions needed when compiling with the unit test, test_atomic: */
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
#include "sim_atomic.h"

/* Forward decl's: */
#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
static SIM_INLINE sim_tailq_elem_t *get_head_node(const sim_tailq_t * const p);
static SIM_INLINE void put_tail_node(sim_tailq_t *p, sim_tailq_head_t *node);
static SIM_INLINE int do_update_head(sim_tailq_t *p, sim_tailq_elem_t *new_elem, sim_tailq_tail_t new_tail);
static SIM_INLINE int do_update_tail(sim_tailq_t *p, sim_tailq_elem_t *new_tail, sim_tailq_head_t *next_insert);
#endif

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Initialization, destruction:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

void sim_tailq_init(sim_tailq_t *p)
{
    p->head = NULL;

#if !NEED_VALUE_MUTEX
    put_tail_node(p, &p->head);
    sim_atomic_init(&p->n_elements);
#else
    pthread_mutexattr_t recursive;

    p->tail = &p->head;

    /* This mutex is paired with the element counter. */
    p->paired = 0;
    p->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));

    pthread_mutexattr_init (&recursive);
    pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(p->lock, &recursive);
    pthread_mutexattr_destroy(&recursive);

    sim_atomic_paired_init(&p->n_elements, p->lock);
#endif

    sim_atomic_put(&p->n_elements, 0);
}

void sim_tailq_paired_init(sim_tailq_t *p, pthread_mutex_t *mutex)
{
    p->head = NULL;

#if !NEED_VALUE_MUTEX
    put_tail_node(p, &p->head);
    sim_atomic_init(&p->n_elements);
    (void) mutex;
#else
    p->tail = &p->head;
    p->paired = 1;
    p->lock = mutex;
    sim_atomic_paired_init(&p->n_elements, p->lock);
#endif

    sim_atomic_put(&p->n_elements, 0);
}

void sim_tailq_destroy(sim_tailq_t *p, int free_elems)
{
    sim_tailq_elem_t *q;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    int did_xchg;

    do {
        q = p->head;
        did_xchg = do_update_head(p, NULL, NULL);
    } while (!did_xchg);
#else
    pthread_mutex_lock(p->lock);
    q = p->head;
    p->head = NULL;
    pthread_mutex_unlock(p->lock);
    
    if (!p->paired) {
        pthread_mutex_destroy(p->lock);
        free(p->lock);
    }

    p->paired = 0;
#endif

    /* Intentionally set to NULL so that any further use will result in a
     * NULL dereference. */
    p->tail = NULL;

    /* Deallocate the list elements (contents are the caller's responsibility.) */
    while (q != NULL) {
        sim_tailq_elem_t *q_next = q->next;
        if (free_elems)
            free(q->elem);
        free(q);
        q = q_next;
    }

    sim_atomic_destroy(&p->n_elements);
}

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Basic operations:
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

/* Queue an element at the tail queue's head.
 *
 * Returns:
 * NULL: calloc() error.
 * Otherwise, Returns the tail queue with the element inserted.
 */
sim_tailq_t *sim_tailq_insert_head(sim_tailq_t *p, void *elem)
{
    sim_tailq_elem_t *new_head = (sim_tailq_elem_t *) calloc(1, sizeof(sim_tailq_elem_t));

    if (new_head == NULL)
        return NULL;

    new_head->elem = elem;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    int did_xchg;

    do {
        /* Don't move the tail unless this is the first element. */
        new_head->next = p->head;
        did_xchg = do_update_head(p, new_head, (p->head != NULL ? p->tail : &new_head->next));
    } while (!did_xchg);

    sim_atomic_inc(&p->n_elements);
#else /* NEED_VALUE_LOCK */
    pthread_mutex_lock(p->lock);
    if (p->head == NULL)
        p->tail = &new_head->next;
    new_head->next = p->head;
    p->head = new_head;
    sim_atomic_inc(&p->n_elements);
    pthread_mutex_unlock(p->lock);
#endif

    return p;
}

sim_tailq_t *sim_tailq_append(sim_tailq_t *p, void *elem)
{
    sim_tailq_elem_t  *new_tail = (sim_tailq_elem_t *) calloc(1, sizeof(sim_tailq_elem_t));

    if (new_tail == NULL)
        return NULL;

    new_tail->elem = elem;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    int did_xchg;

    do {
        did_xchg = do_update_tail(p, new_tail, &new_tail->next);
    } while (!did_xchg);

    sim_atomic_inc(&p->n_elements);
#else /* NEED_VALUE_LOCK */
    pthread_mutex_lock(p->lock);
    (*p->tail) = new_tail;
    p->tail = &new_tail->next;
    sim_atomic_inc(&p->n_elements);
    pthread_mutex_unlock(p->lock);
#endif

    return p;
}

sim_tailq_t *sim_tailq_take(sim_tailq_t *src, sim_tailq_t *dst)
{
    /* Empty source? */
    if (sim_tailq_empty(src))
        return dst;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    int did_xchg;

    do {
        dst->head = src->head;
        dst->tail = src->tail;
        /* Set src's head to NULL. */
        did_xchg = do_update_head(src, NULL, NULL);
    } while (!did_xchg);

    /* Update element counts. */
    sim_atomic_put(&dst->n_elements, sim_atomic_get(&src->n_elements));
    sim_atomic_put(&src->n_elements, 0);
#else /* NEED_VALUE_LOCK */
    pthread_mutex_lock(src->lock);
    pthread_mutex_lock(dst->lock);
    dst->head = src->head;
    dst->tail = src->tail;
    src->head = NULL;
    src->tail = &src->head;
    sim_atomic_put(&dst->n_elements, sim_atomic_get(&src->n_elements));
    sim_atomic_put(&src->n_elements, 0);
    pthread_mutex_unlock(dst->lock);
    pthread_mutex_unlock(src->lock);
#endif

    return dst;
}

sim_tailq_t *sim_tailq_splice(sim_tailq_t *onto, sim_tailq_t *from)
{
    /* Empty from? */
    if (sim_tailq_empty(from))
        return onto;

    sim_tailq_t tmp;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    int did_xchg;

    /* NULL out from's head pointer, transfer into tmp. */
    do {
        tmp.head = from->head;
        tmp.tail = from->tail;
        did_xchg = do_update_head(from, NULL, NULL);
    } while (!did_xchg);

    /* And splice onto's tail */
    do {
        did_xchg = do_update_tail(onto, tmp.head, tmp.tail);
    } while (!did_xchg);

    /* Update element counts. */
    sim_atomic_add(&onto->n_elements, sim_atomic_get(&from->n_elements));
    sim_atomic_put(&from->n_elements, 0);
#else /* NEED_VALUE_LOCK */
    pthread_mutex_lock(onto->lock);
    pthread_mutex_lock(from->lock);

    (*onto->tail) = from->head;
    onto->tail = from->tail;
    sim_atomic_add(&onto->n_elements, sim_atomic_get(&from->n_elements));

    from->head = NULL;
    from->tail = &from->head;
    sim_atomic_put(&from->n_elements, 0);

    pthread_mutex_unlock(from->lock);
    pthread_mutex_unlock(onto->lock);
#endif

    return onto;
}

void *sim_tailq_dequeue_head(sim_tailq_t *p)
{
    sim_tailq_elem_t *head;

    if (sim_tailq_empty(p))
        return NULL;

#if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
    int did_xchg;

    do {
        head = p->head;
        did_xchg = do_update_head(p, p->head->next, p->tail);
    } while (!did_xchg);
#else /* NEED_VALUE_LOCK */
    pthread_mutex_lock(p->lock);
    head = p->head;
    p->head = p->head->next;
    if (p->head == NULL)
        p->tail = &p->head;
    pthread_mutex_unlock(p->lock);
#endif

    void *elem = head->elem;

    /* Free the element node. */
    free(head);
    sim_atomic_dec(&p->n_elements);

    return elem;
}

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Atomic head/tail manipulation. This is where the complexity to support C11+ standard atomics, traditional GNU/Clang
 * builtins and Windows interlocked intrinsics lies.
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

 #if HAVE_STD_ATOMIC || HAVE_ATOMIC_PRIMS
 /*!
  * Tail queue's head node accessor.
  */
 static SIM_INLINE sim_tailq_elem_t *get_head_node(const sim_tailq_t * const p)
 {
 #if HAVE_STD_ATOMIC
     return p->head;
 #elif HAVE_ATOMIC_PRIMS
 #  if defined(__GNUC__) || defined(__clang__)
     sim_tailq_elem_t *head_node;
 
     __atomic_load(&p->head, &head_node, __ATOMIC_SEQ_CST);
     return head_node;
 #  elif defined(_WIN32) || defined(_WIN64)
 #    if defined(_M_IX86) || defined(_M_X64)
         /* Intel Total Store Ordering shortcut. */
         return p->head;
 #    else
         return InterlockedExchangePointer(&p->head, p->head);
 #    endif
 #  endif
 #endif
 }
 
 /*!
  *
  */
 static SIM_INLINE void put_tail_node(sim_tailq_t *p, sim_tailq_head_t *node)
 {
 #if HAVE_STD_ATOMIC
     p->tail = node;
 #elif HAVE_ATOMIC_PRIMS
 #  if defined(__GNUC__) || defined(__clang__)
     __atomic_store(&p->tail, &node, __ATOMIC_SEQ_CST);
 #  elif defined(_WIN32) || defined(_WIN64)
 #    if defined(_M_IX86) || defined(_M_X64)
         /* Intel Total Store Ordering shortcut. */
         p->tail = node;
 #    else
         return InterlockedExchangePointer(&p->tail, node);
 #    endif
 #  endif
 #endif
 }
 
 /*!
  * Update the tail queue's head node.
  *
  * \param[in] p The tail queue to alter
  * \param[in] new_elem New element node to queue at the head
  * \param[in] new_tail If new_elem == NULL (p will now become an empty tail queue), new_tail points
  *     to &p->head. Otherwise, new_tail is the next insertion point.
  * 
  * \return 0 if the compare/exchange failed (rare), otherwise 1 if the compare/exchange suceeded.
  */
 static SIM_INLINE int do_update_head(sim_tailq_t *p, sim_tailq_elem_t *new_elem, sim_tailq_tail_t new_tail)
 {
     int                did_xchg = -1;
     sim_tailq_elem_t  *q = get_head_node(p);
     /* If the new element is NULL, ensure next insertion is at the tail queue's head. */
     sim_tailq_head_t  *actual_tail = (new_elem != NULL ? new_tail : &p->head);
 
 #  if HAVE_STD_ATOMIC
     did_xchg = atomic_compare_exchange_strong(&p->head, &q, new_elem);
     if (did_xchg && p->tail != actual_tail)
         atomic_store(&p->tail, actual_tail);
 #  elif HAVE_ATOMIC_PRIMS
 #    if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
     did_xchg = __atomic_compare_exchange(&p->head, &q, &new_elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
     if (did_xchg)
         __atomic_store(&p->tail, &actual_tail, __ATOMIC_SEQ_CST);
 #    elif defined(_WIN32) || defined(_WIN64)
     did_xchg = (InterlockedCompareExchangePointer(&p->head, new_elem, q) == q);
     if (did_xchg && p->tail != actual_tail)
         InterlockedExchangePointer((PVOID volatile *) &p->tail, (PVOID) actual_tail);
 #    endif
 #  endif
 
     return did_xchg;
 }
 
 static SIM_INLINE int do_update_tail(sim_tailq_t *p, sim_tailq_elem_t *new_tail, sim_tailq_head_t *next_insert)
 {
     int did_xchg = -1;
     sim_tailq_tail_t  cur_tail = p->tail;
 
     /* Update the list's tail pointer first, then commit the head or next link. */
 
 #if HAVE_STD_ATOMIC
         sim_tailq_head_t *tail_val = atomic_load(&p->tail);
 
         did_xchg = atomic_compare_exchange_strong(&p->tail, &tail_val, next_insert);
         if (did_xchg) {
             atomic_store(cur_tail, new_tail);
         }
 #elif HAVE_ATOMIC_PRIMS
 #  if defined(__ATOMIC_SEQ_CST) && (defined(__GNUC__) || defined(__clang__))
         sim_tailq_head_t *tail_val;
        
         __atomic_load(&p->tail, &tail_val, __ATOMIC_SEQ_CST);
         did_xchg = __atomic_compare_exchange(&p->tail, &tail_val, &next_insert, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
         if (did_xchg)
             __atomic_store(cur_tail, &new_tail, __ATOMIC_SEQ_CST);
 #  elif defined(_WIN32) || defined(_WIN64)
         did_xchg = (InterlockedCompareExchangePointer((PVOID volatile *) &p->tail, (PVOID) next_insert, (PVOID) cur_tail) == cur_tail);
         if (did_xchg) {
             InterlockedExchangePointer(cur_tail, new_tail);
         }
 #  endif
 #endif
 
     return did_xchg;
 }
 #endif
 
