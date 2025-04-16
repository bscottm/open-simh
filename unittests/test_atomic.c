#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <time.h>

/* Using C11+ concurrency? */
#if !defined(__STDC_NO_THREADS__) && __STDC_VERSION__ >= 201112L
#include <threads.h>
#define HAVE_STD_THREADS 1

typedef cnd_t sim_cond_t;
typedef mtx_t sim_mutex_t;
#elif defined(USING_PTHREADS)
#define HAVE_STD_THREADS 0
#endif

/* Definitions from sim_defs.h without all of the overhead. */

#if defined(_MSC_VER)
#define SIM_INLINE _inline
#elif defined(__GNUC__) || defined(__clang__)
#define SIM_INLINE inline
#else
#define SIM_INLINE
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <malloc.h>
#endif

#include <unity.h>
#include "sim_atomic.h"

/* Forward decl's for the tests. */
void os_check_malloc();
void test_insert_head_tail(void);
void test_mixed_inserts(void);
void test_tailq_take_splice(void);
void test_thread_head_tail(void);

/* And Win32 compatibility functions: */
#if defined(WIN_NANOSLEEP) && WIN_NANOSLEEP
typedef int clockid_t;

int nanosleep(const struct timespec*, struct timespec*);
int clock_gettime(clockid_t, struct timespec*);
int clock_getres(clockid_t, struct timespec*);
#endif

/* Nanoseconds per microseconds. */
static const long long NSEC_PER_USEC = 1000000000ll / 1000000ll;

/* Random number generation: Uses the xoshiro128** PRNG. */
/* This is xoshiro128** 1.1, one of our 32-bit all-purpose, rock-solid
   generators. It has excellent speed, a state size (128 bits) that is
   large enough for mild parallelism, and it passes all tests we are aware
   of.

   Note that version 1.0 had mistakenly s[0] instead of s[1] as state
   word passed to the scrambler.

   For generating just single-precision (i.e., 32-bit) floating-point
   numbers, xoshiro128+ is even faster.

   The state must be seeded so that it is not everywhere zero. */
/*  Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */
typedef uint32_t rand_state_t[4];

void rand_init(rand_state_t rand);
uint32_t rand_next(rand_state_t seed);
uint32_t rand_int_range (rand_state_t rand, int begin, int end);

/* Test data: */
#define array_size(arr) (sizeof(arr) / sizeof(arr[0]))

int init_values[] = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10 };
int tail_values[] = { 21, 22, 23, 24, 25, 26, 27, 28, 29, 30 };
int xtra_values[] = { 31, 32, 33, 34 };
int deep_values[] = { 49, 48, 47 };

int expected_1[] = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30 };
int expected_2[] = { 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10 };
int expected_3[] = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34 };

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();

#if HAVE_STD_ATOMIC
    puts("Testing using C11 (and later) standard atomics.");
#elif HAVE_ATOMIC_PRIMS
#  if defined(__clang__)
    /* Clang also defines __GNUC__. Ugh. */
    puts("Testing using Clang compiler builtins.");
#  elif defined(__GNUC__)
    puts("Testing using GNU C compiler atomic builtins.");
#  elif defined(_WIN32) || defined(_WIN64)
    puts("Testing using Windows interlocked intrinsics.");
#  else
    puts("Testing using unknown compler/platform atomic intrinsics.");
#  endif
#else
    puts("Testing using mutex guards.");
#endif

    /* Basic functionality tests: */
    RUN_TEST(test_insert_head_tail);
    RUN_TEST(test_mixed_inserts);
    RUN_TEST(test_tailq_take_splice);

    /* Threaded testing: */
    RUN_TEST(test_thread_head_tail);

    return UNITY_END();
}

void os_check_malloc()
{
#if defined(_WIN32) || defined(_WIN64)
    int hcheck = _heapchk();
    if (hcheck != _HEAPOK) {
        fprintf(stderr, "heap check failed with %d.\n", hcheck);
        exit(99);
    }
#endif
}

void test_insert_head_tail(void)
{
    sim_tailq_t l;
    sim_tailq_elem_t *p;
    size_t i, j;

    /* Head inserts: */
    sim_tailq_init(&l);
    for (i = array_size(init_values); i > 0; /* empty */)
        sim_tailq_insert_head(&l, init_values + --i);

    for (i = 1, j = 0, p = sim_tailq_iter_head(&l); p != NULL; p = sim_tailq_iter_next(p), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(init_values));
        TEST_ASSERT_EQUAL_INT(init_values[j], *((int *) sim_tailq_element(p)));
    }

    TEST_ASSERT_EQUAL(sim_tailq_count(&l), array_size(init_values));

    /* Tail appends: */
    for (i = 0; i < array_size(tail_values); ++i)
        sim_tailq_append(&l, tail_values + i);

    for (i = 1, j = 0, p = sim_tailq_iter_head(&l); p != NULL; p = sim_tailq_iter_next(p), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(expected_1));
        TEST_ASSERT_EQUAL(expected_1[j], *((int *) sim_tailq_element(p)));
    }

    TEST_ASSERT_EQUAL(sim_tailq_count(&l), array_size(init_values) + array_size(tail_values));

    sim_tailq_destroy(&l, 0);
    os_check_malloc();
}

void test_mixed_inserts(void)
{
    sim_tailq_t l;
    sim_tailq_elem_t const *p;
    size_t i, j;

    sim_tailq_init(&l);

    /* Append a bunch of values: */
    for (i = 0; i < array_size(init_values); ++i)
        sim_tailq_append(&l, init_values + i);

    /* Then insert at the head. */
    for (i = array_size(tail_values); i > 0; /* empty */)
        sim_tailq_insert_head(&l, tail_values + --i);

    for (i = 1, j = 0, p = sim_tailq_iter_head(&l); p != NULL; p = sim_tailq_iter_next(p), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(expected_2));
        TEST_ASSERT_EQUAL_INT(expected_2[j], *((int *) sim_tailq_element(p)));
    }

    TEST_ASSERT_EQUAL(sim_tailq_count(&l), array_size(expected_2));

    sim_tailq_destroy(&l, 0);
    os_check_malloc();
}

void test_tailq_take_splice(void)
{
    sim_tailq_t l, l2;
    sim_tailq_elem_t const *p;
    size_t i, j;

    sim_tailq_init(&l);
    sim_tailq_init(&l2);

    for (i = 0; i < array_size(tail_values); ++i)
        sim_tailq_append(&l, tail_values + i);

    TEST_ASSERT_EQUAL_MESSAGE(sim_tailq_take(&l, &l2), &l2, "sim_tailq_take did not return &l2");
    TEST_ASSERT_EQUAL_MESSAGE(l.head, NULL, "sim_tailq_take: l.head not NULL");
    TEST_ASSERT_EQUAL_MESSAGE(l.tail, &l.head, "sim_tailq_take: l.tail does not point to l.head");
    TEST_ASSERT_EQUAL(sim_tailq_count(&l2), array_size(tail_values));
    TEST_ASSERT_EQUAL(sim_tailq_count(&l), 0);

    for (i = 1, j = 0, p = sim_tailq_iter_head(&l2); p != NULL; p = sim_tailq_iter_next(p), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(tail_values));
        TEST_ASSERT_EQUAL_INT(tail_values[j], *((int *) sim_tailq_element(p)));
    }

    os_check_malloc();

    for (i = 0; i < array_size(init_values); ++i)
        sim_tailq_append(&l, init_values + i);

    TEST_ASSERT_EQUAL_MESSAGE(sim_tailq_splice(&l, &l2), &l, "sim_tailq_splice did not return &l");
    TEST_ASSERT_EQUAL_MESSAGE(l2.head, NULL, "sim_tailq_splice: l2.head not NULL");
    TEST_ASSERT_EQUAL_MESSAGE(l2.tail, &l2.head, "sim_tailq_splice: l2.tail does not point to l2.head");

    for (i = 1, j = 0, p = sim_tailq_iter_head(&l); p != NULL; p = sim_tailq_iter_next(p), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(expected_1));
        TEST_ASSERT_EQUAL_INT(expected_1[j], *((int *) sim_tailq_element(p)));
    }

    for (i = 0; i < array_size(xtra_values); ++i)
        sim_tailq_append(&l, &xtra_values[i]);

    for (i = 1, j = 0, p = sim_tailq_iter_head(&l); p != NULL; p = sim_tailq_iter_next(p), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(expected_3));
        TEST_ASSERT_EQUAL_INT(expected_3[j], *((int *) sim_tailq_element(p)));
    }

    TEST_ASSERT_EQUAL_MESSAGE(l2.head, NULL, "sim_tailq_splice (2): l2.head not NULL");
    TEST_ASSERT_EQUAL_MESSAGE(l2.tail, &l2.head, "sim_tailq_splice (2): l2.tail does not point to l2.head");

    sim_tailq_destroy(&l, 0);
    sim_tailq_destroy(&l2, 0);

    os_check_malloc();
}

/*~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
 * Simulate tail queueing, remove from head in a separate thread.
 *
 * Two separate implementations: C11+ standard concurrency library, pthreads. They're almost identical. Useful for
 * how to use the standard concurrency library.
 *~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=*/

/*!
 * Test reader thread state.
 */
 typedef enum {
    READER_INIT,                /*!< Thread initialized, not running. */
    READER_RUNNING,             /*!< Thread running. */
    READER_SHUTDOWN,            /*!< Thread should exit its loop. */
    READER_EXITED               /*!< Thread has exited. */
} head_tail_state_t;

typedef struct {
    sim_tailq_t *tailq;         /*!< The tail queue being operated upon. */
    sim_atomic_value_t state;   /*!< The reader thread's state. */
    rand_state_t prng;          /*!< Pseudo-random number generator state. */

#if HAVE_STD_THREADS
    cnd_t empty_queue_cond;
    mtx_t empty_queue_mtx;
    cnd_t startup_cond;
    mtx_t startup_mtx;
#elif defined(USING_PTHREADS)
    pthread_cond_t empty_queue_cond;
    pthread_mutex_t empty_queue_mtx;
    pthread_cond_t startup_cond;
    pthread_mutex_t startup_mtx;
#endif
} head_tail_startup_t;

static const int reader_iter_limit = 10244;
static int reader_test_elem = 0x0abc1234;

#if HAVE_STD_THREADS
#define THREAD_FUNC_DECL(FUNC) int FUNC(void *arg)
#elif defined(USING_PTHREADS)
#define THREAD_FUNC_DECL(FUNC) void *FUNC(void *arg)
#endif

THREAD_FUNC_DECL(dequeue_head_reader);

void test_thread_head_tail(void)
{
    sim_tailq_t tailq;
    rand_state_t prng;

    head_tail_startup_t info = {
        .tailq = &tailq
    };

    sim_tailq_init(info.tailq);
    sim_atomic_init(&info.state);
    sim_atomic_put(&info.state, READER_INIT);
    rand_init(info.prng);

#if HAVE_STD_THREADS
    thrd_t reader;

    cnd_init(&info.empty_queue_cond);
    mtx_init(&info.empty_queue_mtx, mtx_plain);
    cnd_init(&info.startup_cond);
    mtx_init(&info.startup_mtx, mtx_plain);

    mtx_lock(&info.startup_mtx);
    thrd_create(&reader, dequeue_head_reader, &info);
    cnd_wait(&info.startup_cond, &info.startup_mtx);
    mtx_unlock(&info.startup_mtx);
#elif defined(USING_PTHREADS)
    pthread_t reader;

    pthread_cond_init(&info.empty_queue_cond, NULL);
    pthread_mutex_init(&info.empty_queue_mtx, NULL);
    pthread_cond_init(&info.startup_cond, NULL);
    pthread_mutex_init(&info.startup_mtx, NULL);

    pthread_mutex_lock(&info.startup_mtx);
    pthread_create(&reader, NULL, dequeue_head_reader, &info);
    pthread_cond_wait(&info.startup_cond, &info.startup_mtx);
    pthread_mutex_unlock(&info.startup_mtx);
#endif

    TEST_ASSERT_EQUAL(sim_atomic_get(&info.state), READER_RUNNING);

    int i;
    struct timespec delay100us = {
        .tv_sec = 0,
        .tv_nsec = (long) (200ll * NSEC_PER_USEC)
    };

    for (i = 0; i < reader_iter_limit; /* empty */) {
        uint32_t burst = rand_int_range(prng, 1, 16);

        while (burst > 0) {
            if ((i % 1000) == 0) {
                int queue_count = 0;
                sim_tailq_elem_t *p;
              
                for (p = sim_tailq_iter_head(&tailq); p != NULL; p = sim_tailq_iter_next(p))
                  ++queue_count;
              
                printf("%5d writer (%" PRIsim_atomic ", %d)...\n", i, sim_tailq_count(&tailq), queue_count);
                fflush(stdout);
            }

            sim_tailq_append(&tailq, &reader_test_elem);
            --burst;
            ++i;
        }

        if (sim_tailq_count(&tailq) > 1) {
#if HAVE_STD_THREADS
            mtx_lock(&info.empty_queue_mtx);
            cnd_signal(&info.empty_queue_cond);
            mtx_unlock(&info.empty_queue_mtx);
#elif defined(USING_PTHREADS)
            pthread_mutex_lock(&info.empty_queue_mtx);
            pthread_cond_signal(&info.empty_queue_cond);
            pthread_mutex_unlock(&info.empty_queue_mtx);
#endif
        }

        nanosleep(&delay100us, NULL);
    }

    printf("%5d writer done.\n", i);

    sim_atomic_put(&info.state, READER_SHUTDOWN);
#if HAVE_STD_THREADS
    mtx_lock(&info.empty_queue_mtx);
    cnd_signal(&info.empty_queue_cond);
    mtx_unlock(&info.empty_queue_mtx);
#elif defined(USING_PTHREADS)
    pthread_mutex_lock(&info.empty_queue_mtx);
    pthread_cond_signal(&info.empty_queue_cond);
    pthread_mutex_unlock(&info.empty_queue_mtx);
#endif


#if HAVE_STD_THREADS
    int reader_exitval;

thrd_join(reader, &reader_exitval);
#elif defined(USING_PTHREADS)
    void *reader_exitval;

    pthread_join(reader, &reader_exitval);
#endif

    TEST_ASSERT_EQUAL(READER_EXITED, sim_atomic_get(&info.state));
    TEST_ASSERT_EQUAL(0, sim_tailq_count(&tailq));
    TEST_ASSERT_TRUE(sim_tailq_empty(&tailq));

#if HAVE_STD_THREADS
    cnd_destroy(&info.startup_cond);
    mtx_destroy(&info.startup_mtx);
    cnd_destroy(&info.empty_queue_cond);
    mtx_destroy(&info.empty_queue_mtx);
#elif defined(USING_PTHREADS)
    pthread_cond_destroy(&info.startup_cond);
    pthread_mutex_destroy(&info.startup_mtx);
    pthread_cond_destroy(&info.empty_queue_cond);
    pthread_mutex_destroy(&info.empty_queue_mtx);
#endif

    sim_tailq_destroy(&tailq, 0);
}

THREAD_FUNC_DECL(dequeue_head_reader)
{
    head_tail_startup_t *info = (head_tail_startup_t *) arg;
    uint32_t burst = rand_int_range(info->prng, 1, 11);
    int iter = 0;
    int queue_count = 0;
    sim_tailq_elem_t *p;

    sim_atomic_put(&info->state, READER_RUNNING);

#if HAVE_STD_THREADS
    mtx_lock(&info->startup_mtx);
    cnd_signal(&info->startup_cond);
    mtx_unlock(&info->startup_mtx);
#elif defined(USING_PTHREADS)
    pthread_mutex_lock(&info->startup_mtx);
    pthread_cond_signal(&info->startup_cond);
    pthread_mutex_unlock(&info->startup_mtx);
#endif

    while (sim_atomic_get(&info->state) == READER_RUNNING) {
        int *item = (int *) sim_tailq_dequeue_head(info->tailq);
        
        if (item == NULL) {
#if HAVE_STD_THREADS
            mtx_lock(&info->empty_queue_mtx);
            cnd_wait(&info->empty_queue_cond, &info->empty_queue_mtx);
            mtx_unlock(&info->empty_queue_mtx);
#elif defined(USING_PTHREADS)
            pthread_mutex_unlock(&info->empty_queue_mtx);
            pthread_cond_wait(&info->empty_queue_cond, &info->empty_queue_mtx);
            pthread_mutex_unlock(&info->empty_queue_mtx);
#endif

            burst = rand_int_range(info->prng, 1, 11);
        } else {
            TEST_ASSERT_EQUAL(reader_test_elem, *item);

            if ((iter % 1000) == 0) {
                queue_count = 0;
                for (p = sim_tailq_iter_head(info->tailq); p != NULL; p = sim_tailq_iter_next(p))
                  ++queue_count;
              
                printf("%5d reader (%" PRIsim_atomic ", %d)...\n", iter, sim_tailq_count(info->tailq), queue_count);
                fflush(stdout);
            }

            long long delay_nsecs = (15 * rand_int_range(info->prng, 1, 7)) * NSEC_PER_USEC;

            TEST_ASSERT(delay_nsecs < LONG_MAX);

            if (--burst == 0) {
                struct timespec read_delay = {
                    .tv_sec = 0,
                    .tv_nsec = (long) delay_nsecs
                };
            
                nanosleep(&read_delay, NULL);
                burst = rand_int_range(info->prng, 1, 11);
            }

            ++iter;
        }
    }

    while (sim_tailq_count(info->tailq) > 0) {
        sim_tailq_dequeue_head(info->tailq);
        ++iter;
    }
  
    queue_count = 0;
    for (p = sim_tailq_iter_head(info->tailq); p != NULL; p = sim_tailq_iter_next(p))
      ++queue_count;
  
    printf("%5d reader (%" PRIsim_atomic ", %d)...\n", iter, sim_tailq_count(info->tailq), queue_count);
    fflush(stdout);

    sim_atomic_put(&info->state, READER_EXITED);

    return 0;
}

#if defined(WIN_NANOSLEEP) && WIN_NANOSLEEP
// nanosleep(), clock_gettime(), clock_getres() for Windows

#ifndef __has_c_attribute
#define __has_c_attribute(x) 0
#endif

#if __has_c_attribute(maybe_unused)
#define MAYBE_UNUSED [[maybe_unused]]
#else
#define MAYBE_UNUSED
#endif

static const long long NSEC_PER_SEC = 1000000000L;
static const long long TIMER_QUANTUM = 100L;

int nanosleep(const struct timespec *ts, MAYBE_UNUSED struct timespec *rem)
{
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (timer == NULL)
        return -1;

    // SetWaitableTimer() defines interval in 100ns units.
    // negative indicates relative time.
    time_t sec = ts->tv_sec + ts->tv_nsec / NSEC_PER_SEC;
    long nsec = ts->tv_nsec % NSEC_PER_SEC;

    LARGE_INTEGER delay;
    delay.QuadPart = -(sec * NSEC_PER_SEC + nsec) / TIMER_QUANTUM;
    BOOL ok = SetWaitableTimer(timer, &delay, 0, NULL, NULL, FALSE) &&
              WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0;

    CloseHandle(timer);

    return (ok ? 0 :-1);
}


int clock_gettime(MAYBE_UNUSED clockid_t clk_id, struct timespec *t){
  LARGE_INTEGER count;

  if(!QueryPerformanceCounter(&count))
    return -1;

  LARGE_INTEGER freq;
  if(!QueryPerformanceFrequency(&freq))
    return -1;

  t->tv_sec = count.QuadPart / freq.QuadPart;
  t->tv_nsec = (long) (((count.QuadPart % freq.QuadPart) * NSEC_PER_SEC) / freq.QuadPart);

  return 0;
}


int clock_getres(MAYBE_UNUSED clockid_t clk_id, struct timespec *res){
  LARGE_INTEGER freq;
  if(!QueryPerformanceFrequency(&freq))
    return -1;

  res->tv_sec = 0;
  res->tv_nsec = (long) (NSEC_PER_SEC / freq.QuadPart);

  return 0;
}
#endif

static inline uint32_t rand_rotl(const uint32_t x, int k)
{
	return (x << k) | (x >> (32 - k));
}

uint32_t rand_next(rand_state_t seed)
{
	const uint32_t result = rand_rotl(seed[1] * 5, 7) * 9;

	const uint32_t t = seed[1] << 9;

	seed[2] ^= seed[0];
	seed[3] ^= seed[1];
	seed[1] ^= seed[2];
	seed[0] ^= seed[3];

	seed[2] ^= t;

	seed[3] = rand_rotl(seed[3], 11);

	return result;
}

void rand_init(rand_state_t rand)
{
#if !defined(_WIN32)
#  if defined(HAVE_CLOCK_GETTIME)
   struct timespec now;

   clock_gettime(CLOCK_REALTIME, &now);

    rand[0] = now.tv_sec;
    rand[1] = now.tv_nsec;
#  elif defined(HAVE_GETTIMEOFDAY)
    struct timeval now;

    gettimeofday (&now, NULL);

    rand[0] = now.tv_sec;
    rand[1] = now.tv_usec;
#  endif

    rand[2] = getpid ();
#else
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );

    time =  ((uint64_t) file_time.dwLowDateTime );
    time += ((uint64_t) file_time.dwHighDateTime) << 32;

    rand[0] = (uint32_t) ((time - EPOCH) / 10000000L);
    rand[1] = (uint32_t) (system_time.wMilliseconds * 1000);
    rand[2] = (uint32_t) GetCurrentProcessId();
#endif

    rand[3] = 0;
}

uint32_t rand_int_range (rand_state_t rand, int begin, int end)
{
    return ((rand_next(rand) % (end - begin)) + begin);
}
