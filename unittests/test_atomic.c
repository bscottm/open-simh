#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

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
#if HAVE_STD_ATOMIC
    puts("Testing using C11 and later standard atomics.");
#elif HAVE_ATOMIC_PRIMS
#  if defined(__GNUC__) || defined(__clang__)
    puts("Testing using GNU C/Clang compiler atomic builtins.");
#  elif defined(_WIN32) || defined(_WIN64)
    puts("Testing using Windows interlocked intrinsics.");
#  else
    puts("Testing using unknown compler/platform atomic intrinsics.");
#  endif
#else
    puts("Testing using mutex guards.");
#endif

    UNITY_BEGIN();

    RUN_TEST(test_insert_head_tail);
    RUN_TEST(test_mixed_inserts);
    RUN_TEST(test_tailq_take_splice);

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
