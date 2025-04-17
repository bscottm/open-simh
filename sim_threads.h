
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

#if defined(C11_CONCURRENCY_LIB) && C11_CONCURRENCY_LIB
#include <threads.h>
#define HAVE_STD_THREADS 1
#define THREAD_FUNC_DECL(FUNC) int FUNC(void *arg)
#define THREAD_FUNC_DEFN(FUNC) int FUNC(void *arg)

typedef cnd_t sim_cond_t;
typedef mtx_t sim_mutex_t;
#elif defined(USING_PTHREADS) && USING_PTHREADS
#define HAVE_STD_THREADS 0
#define THREAD_FUNC_DECL(FUNC) void *FUNC(void *arg)
#define THREAD_FUNC_DEFN(FUNC) void *FUNC(void *arg)

typedef pthread_cond_t sim_cond_t;
typedef pthread_mutex_t sim_mutex_t;
#endif

static SIM_INLINE int sim_cond_init(sim_cond_t *cond)
{
#if HAVE_STD_THREADS
    return cnd_init(cond);
#elif defined(USING_PTHREADS) && USING_PTHREADS
    return pthread_cond_init(cond, NULL);
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
