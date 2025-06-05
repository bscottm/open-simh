#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include <time.h>

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
#else
#include <unistd.h>
#endif

#include <unity.h>
#include "sim_tailq.h"
#include "sim_threads.h"

/* Forward decl's for the tests. */
void os_check_malloc();
void test_tailq_enqueue(void);
void test_tailq_dequeue(void);
void test_tailq_enqueue_xform(void);
void test_thread_head_tail_nodelay(void);
void test_thread_head_tail_100_200(void);
void test_thread_head_tail_200_100(void);

static inline uint32_t sim_tailq_actual(const sim_tailq_t *tailq);

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
#if !defined(array_size)
#define array_size(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

int init_values[] = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 21, 32, 43, 54, 65, 76, 87, 98, 47, 22 };

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
    RUN_TEST(test_tailq_enqueue);
    RUN_TEST(test_tailq_dequeue);
    RUN_TEST(test_tailq_enqueue_xform);

    /* Threaded testing: */
    RUN_TEST(test_thread_head_tail_nodelay);
    /* Faster writer, slower reader. */
    RUN_TEST(test_thread_head_tail_100_200);
    /* Slower writer, faster reader. */
    RUN_TEST(test_thread_head_tail_200_100);

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

/* Test basic enqueuing -- queue up a bunch of values, make sure they're enqueued in the
 * expected order. */
void test_tailq_enqueue()
{
    sim_tailq_t tailq;
    sim_tailq_elem_t const *p;
    size_t i, j;

    sim_tailq_init(&tailq);

    for (i = 0; i < array_size(init_values); ++i)
        sim_tailq_enqueue(&tailq, init_values + i);

    for (i = 1, j = 0, p = sim_tailq_head(&tailq); !sim_tailq_at_tail(p, &tailq); p = sim_tailq_next(p, &tailq), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(init_values));
        TEST_ASSERT_EQUAL_INT(init_values[j], *((int *) sim_tailq_item(p)));
    }

    TEST_ASSERT_EQUAL(sim_tailq_count(&tailq), array_size(init_values));

    sim_tailq_destroy(&tailq, 0);
    os_check_malloc();
}

/* Enqueue a bunch of values, then dequeue them and ensure they dequeue in the proper
 * order. */
void test_tailq_dequeue(void)
{
    sim_tailq_t tailq;
    void *thing;
    size_t i;

    sim_tailq_init(&tailq);

    /* Enqueue a bunch of values: */
    for (i = 0; i < array_size(init_values); ++i)
        sim_tailq_enqueue(&tailq, init_values + i);

    for (i = 0, thing = sim_tailq_dequeue(&tailq); thing != NULL; thing = sim_tailq_dequeue(&tailq), i++) {
        TEST_ASSERT_FALSE(i >= array_size(init_values));
        TEST_ASSERT_EQUAL_INT(init_values[i], *((int *) thing));
    }

    TEST_ASSERT_EQUAL(0, sim_tailq_count(&tailq));

    sim_tailq_destroy(&tailq, 0);
    os_check_malloc();
}

/* Transform function used by test_tailq_enqueue_xform(). */
static sim_tailq_item_t xform_func(sim_tailq_item_t item, void *item_arg)
{
    int *int_elem = (int *) item;

    if (int_elem == NULL) {
        int_elem = (int *) malloc(sizeof(int));
        TEST_ASSERT_NOT_NULL_MESSAGE(int_elem, "NULL int_elem");
    }

    *int_elem = *((int *) item_arg);
    return int_elem;
}

/* Test basic enqueuing with the function transform. */
void test_tailq_enqueue_xform()
{
    sim_tailq_t tailq;
    sim_tailq_elem_t const *p;
    size_t i, j;

    sim_tailq_init(&tailq);

    for (i = 0; i < array_size(init_values); ++i)
        sim_tailq_enqueue_xform(&tailq, xform_func, init_values + i);

    for (i = 1, j = 0, p = sim_tailq_head(&tailq); !sim_tailq_at_tail(p, &tailq); p = sim_tailq_next(p, &tailq), i++, j++) {
        TEST_ASSERT_FALSE(j >= array_size(init_values));
        TEST_ASSERT_EQUAL_INT(init_values[j], *((int *) sim_tailq_item(p)));
    }

    TEST_ASSERT_EQUAL(sim_tailq_count(&tailq), array_size(init_values));

    sim_tailq_destroy(&tailq, 0);
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
    struct timespec *delay;     /*!< Reader delay */

    sim_cond_t empty_queue_cond;
    sim_mutex_t empty_queue_mtx;
    sim_cond_t startup_cond;
    sim_mutex_t startup_mtx;
} head_tail_startup_t;

static const int reader_iter_limit = 10244;
static int reader_test_elem = 0x0abc1234;

THREAD_FUNC_DECL(dequeue_head_reader);
void enqueue_tail_writer(struct timespec *wr_delay, struct timespec *rd_delay);

void test_thread_head_tail_nodelay(void)
{
    struct timespec null_delay = {
        .tv_sec = 0,
        .tv_nsec = 0
    };

    enqueue_tail_writer(&null_delay, &null_delay);
}

void test_thread_head_tail_100_200(void)
{
    struct timespec delay100us = {
        .tv_sec = 0,
        .tv_nsec = (long) (100ll * NSEC_PER_USEC)
    };
    struct timespec delay200us = {
        .tv_sec = 0,
        .tv_nsec = (long) (200ll * NSEC_PER_USEC)
    };

    enqueue_tail_writer(&delay100us, &delay200us);
}

void test_thread_head_tail_200_100(void)
{
    struct timespec delay100us = {
        .tv_sec = 0,
        .tv_nsec = (long) (100ll * NSEC_PER_USEC)
    };
    struct timespec delay200us = {
        .tv_sec = 0,
        .tv_nsec = (long) (200ll * NSEC_PER_USEC)
    };

    enqueue_tail_writer(&delay200us, &delay100us);
}

void enqueue_tail_writer(struct timespec *wr_delay, struct timespec *rd_delay)
{
    sim_tailq_t tailq;
    rand_state_t prng;

    head_tail_startup_t info = {
        .tailq = &tailq,
        .delay = rd_delay
    };

    sim_tailq_init(info.tailq);
    sim_atomic_init(&info.state);
    sim_atomic_put(&info.state, READER_INIT);
    rand_init(info.prng);

    sim_cond_init(&info.empty_queue_cond);
    sim_mutex_init(&info.empty_queue_mtx);
    sim_cond_init(&info.startup_cond);
    sim_mutex_init(&info.startup_mtx);

    sim_thread_t reader;

    sim_mutex_lock(&info.startup_mtx);
    sim_thread_create(&reader, dequeue_head_reader, &info);
    sim_cond_wait(&info.startup_cond, &info.startup_mtx);
    sim_mutex_unlock(&info.startup_mtx);

    TEST_ASSERT_EQUAL(sim_atomic_get(&info.state), READER_RUNNING);

    int i;

    for (i = 0; i < reader_iter_limit; /* empty */) {
        uint32_t burst = rand_int_range(prng, 1, 16);

        while (burst > 0) {
            if ((i % 1000) == 0) {
                printf("%5d writer (%" PRIsim_atomic ", %u)...\n", i, sim_tailq_count(&tailq), sim_tailq_actual(&tailq));
                fflush(stdout);
            }

            sim_tailq_enqueue(&tailq, &reader_test_elem);
            --burst;
            ++i;
        }

        if (sim_tailq_count(&tailq) > 1) {
            sim_mutex_lock(&info.empty_queue_mtx);
            sim_cond_signal(&info.empty_queue_cond);
            sim_mutex_unlock(&info.empty_queue_mtx);
        }

        if (wr_delay->tv_sec > 0 || wr_delay->tv_nsec > 0)
            nanosleep(wr_delay, NULL);
    }

    printf("%5d writer done.\n", i);

    sim_atomic_put(&info.state, READER_SHUTDOWN);
    sim_mutex_lock(&info.empty_queue_mtx);
    sim_cond_signal(&info.empty_queue_cond);
    sim_mutex_unlock(&info.empty_queue_mtx);

    sim_thread_exit_t reader_exitval;

    sim_thread_join(reader, &reader_exitval);

    TEST_ASSERT_EQUAL(READER_EXITED, sim_atomic_get(&info.state));
    TEST_ASSERT_EQUAL_MESSAGE(0, sim_tailq_count(&tailq), "tailq count != 0");
    TEST_ASSERT_TRUE_MESSAGE(sim_tailq_empty(&tailq), "tailq not empty.");

    sim_cond_destroy(&info.startup_cond);
    sim_mutex_destroy(&info.startup_mtx);
    sim_cond_destroy(&info.empty_queue_cond);
    sim_mutex_destroy(&info.empty_queue_mtx);

    sim_tailq_destroy(&tailq, 0);
}

THREAD_FUNC_DEFN(dequeue_head_reader)
{
    const int burst_max = 11;
    head_tail_startup_t *info = (head_tail_startup_t *) arg;
    uint32_t burst = rand_int_range(info->prng, 1, burst_max);
    int iter = 0;

    sim_atomic_put(&info->state, READER_RUNNING);

    sim_mutex_lock(&info->startup_mtx);
    sim_cond_signal(&info->startup_cond);
    sim_mutex_unlock(&info->startup_mtx);

    while (sim_atomic_get(&info->state) == READER_RUNNING) {
        int *item = (int *) sim_tailq_dequeue(info->tailq);

        if (item == NULL) {
            sim_mutex_lock(&info->empty_queue_mtx);
            sim_cond_wait(&info->empty_queue_cond, &info->empty_queue_mtx);
            sim_mutex_unlock(&info->empty_queue_mtx);

            burst = rand_int_range(info->prng, 1, burst_max);
        } else {
            TEST_ASSERT_EQUAL(reader_test_elem, *item);

            if ((iter % 1000) == 0) {
                printf("%5d reader (%" PRIsim_atomic ", %u)...\n", iter, sim_tailq_count(info->tailq), sim_tailq_actual(info->tailq));
                fflush(stdout);
            }

            if (--burst == 0) {
                if (info->delay->tv_sec > 0 || info->delay->tv_nsec > 0)
                    nanosleep(info->delay, NULL);
                burst = rand_int_range(info->prng, 1, burst_max);
            }

            ++iter;
        }
    }

    while (sim_tailq_count(info->tailq) > 0) {
        if (sim_tailq_dequeue(info->tailq) != NULL) {
            if ((++iter % 1000) == 0) {
                printf("%5d reader (%" PRIsim_atomic ", %u)...\n", iter, sim_tailq_count(info->tailq), sim_tailq_actual(info->tailq));
                fflush(stdout);
            }
        } else {
	    printf("%5d reader (%" PRIsim_atomic ", %u)...\n", iter, sim_tailq_count(info->tailq), sim_tailq_actual(info->tailq));
	    fflush(stdout);
            TEST_FAIL_MESSAGE("reader - residual: Dequeue head, no more elements.");
            break;
        }
    }

    printf("%5d reader (%" PRIsim_atomic ", %u)...\n", iter, sim_tailq_count(info->tailq), sim_tailq_actual(info->tailq));
    fflush(stdout);

    sim_atomic_put(&info->state, READER_EXITED);

    return 0;
}

uint32_t sim_tailq_actual(const sim_tailq_t *tailq)
{
    uint32_t queue_count = 0;
    const sim_tailq_elem_t *p;

    for (p = sim_tailq_head(tailq); !sim_tailq_at_tail(p, tailq); p = sim_tailq_next(p, tailq))
      ++queue_count;

    return queue_count;
}

#if defined(WIN_NANOSLEEP) && WIN_NANOSLEEP
// nanosleep(), clock_gettime(), clock_getres() for Windows

#if !defined(__has_c_attribute)
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
