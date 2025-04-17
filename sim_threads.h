
#if !defined(SIM_THREADS_H)
/*!
 * \file sim_threads.h
 * \brief SIMH thread support wrappers
 * 
 * 
 */

/*!
 * \def THREAD_FUNC_DECL(FUNC)
 * \brief Thread function declaration macro.
 */
/*!
 * \def THREAD_FUNC_DEFN(FUNC)
 * \brief Thread function definition macro.
 */
/*!
 * \typedef sim_cond_t
 * \brief Type definition for condition variables.
 */

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#  include <threads.h>
#  define HAVE_STD_THREADS 1
#  define THREAD_FUNC_DECL(FUNC) int FUNC(void *arg)
#  define THREAD_FUNC_DEFN(FUNC) int FUNC(void *arg)

typedef int    (*sim_thread_fn)(void *arg);
typedef int    sim_thread_exit_t;
typedef thrd_t sim_thread_t;   
typedef cnd_t  sim_cond_t;
typedef mtx_t  sim_mutex_t;
#elif defined(USING_PTHREADS) && USING_PTHREADS
#  include <pthread.h>

#  define HAVE_STD_THREADS 0
#  define THREAD_FUNC_DECL(FUNC) void *FUNC(void *arg)
#  define THREAD_FUNC_DEFN(FUNC) void *FUNC(void *arg)

typedef void           *(*sim_thread_fn)(void *arg);
typedef void           *sim_thread_exit_t;
typedef pthread_t       sim_thread_t;
typedef pthread_cond_t  sim_cond_t;
typedef pthread_mutex_t sim_mutex_t;
#else
#error "No standard threads or pthreads?"
#endif

static SIM_INLINE int sim_thread_create(sim_thread_t *thread_id, sim_thread_fn func, void *arg)
{
#if HAVE_STD_THREADS
    return thrd_create(thread_id, func, arg);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    return pthread_create(thread_id, NULL, func, arg);
#endif
}

static SIM_INLINE int sim_thread_join(sim_thread_t thread_id, sim_thread_exit_t *exit_val)
{
#if HAVE_STD_THREADS
    return thrd_join(thread_id, exit_val);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    return pthread_join(thread_id, exit_val);
#endif
}

static SIM_INLINE int sim_cond_init(sim_cond_t *cond)
{
#if HAVE_STD_THREADS
    return cnd_init(cond);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    return pthread_cond_init(cond, NULL);
#endif
}

static SIM_INLINE void sim_cond_destroy(sim_cond_t *cond)
{
#if HAVE_STD_THREADS
    cnd_destroy(cond);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    pthread_cond_destroy(cond);
#endif
}

static SIM_INLINE int sim_cond_signal(sim_cond_t *cond)
{
#if HAVE_STD_THREADS
    return cnd_signal(cond);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    return pthread_cond_signal(cond);
#endif
}

static SIM_INLINE int sim_cond_wait(sim_cond_t *cond, sim_mutex_t *mtx)
{
#if HAVE_STD_THREADS
    return cnd_wait(cond, mtx);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    return pthread_cond_wait(cond, mtx);
#endif
}
static SIM_INLINE int sim_mutex_init(sim_mutex_t *mtx)
{
#if HAVE_STD_THREADS
    return mtx_init(mtx, mtx_plain);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    return pthread_mutex_init(mtx, NULL);
#endif
}

static SIM_INLINE int sim_mutex_recursive(sim_mutex_t *mtx)
{
#if HAVE_STD_THREADS
    return mtx_init(mtx, mtx_plain | mtx_recursive);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    pthread_mutexattr_t recursive;
    int retval;

    pthread_mutexattr_init (&recursive);
    pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE);
    retval = pthread_mutex_init(mtx, &recursive);
    pthread_mutexattr_destroy(&recursive);
    return retval;
#endif
}

static SIM_INLINE void sim_mutex_lock(sim_mutex_t *mtx)
{
#if HAVE_STD_THREADS
    mtx_lock(mtx);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    pthread_mutex_lock(mtx);
#endif
}

static SIM_INLINE void sim_mutex_unlock(sim_mutex_t *mtx)
{
#if HAVE_STD_THREADS
    mtx_unlock(mtx);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    pthread_mutex_unlock(mtx);
#endif
}

static SIM_INLINE void sim_mutex_destroy(sim_mutex_t *mtx)
{
#if HAVE_STD_THREADS
    mtx_destroy(mtx);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    pthread_mutex_destroy(mtx);
#endif
}

#define SIM_THREADS_H
#endif