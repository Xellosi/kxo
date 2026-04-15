/* Test infrastructure for kxo
 *
 * Section-grouped output with [OK] / [FAIL] style (adapted from libiui).
 * Include this header in all test-*.c files.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test counters (define in exactly one .c file with TEST_MAIN) */
extern int g_tests_run;
extern int g_tests_passed;
extern int g_tests_failed;
extern int g_verbose;

/* ANSI color codes */
#define ANSI_GREEN "\033[32m"
#define ANSI_RED "\033[31m"
#define ANSI_RESET "\033[0m"

/* Section tracking for grouped test output */
extern int g_section_failed;
extern const char *g_section_name;

#define SECTION_BEGIN(name)                \
    do {                                   \
        g_section_failed = g_tests_failed; \
        g_section_name = (name);           \
    } while (0)

#define SECTION_END()                                                \
    do {                                                             \
        if (g_tests_failed == g_section_failed)                      \
            printf("Test %-40s[ " ANSI_GREEN "OK" ANSI_RESET " ]\n", \
                   g_section_name);                                  \
        else                                                         \
            printf("Test %-40s[ " ANSI_RED "FAIL" ANSI_RESET " ]\n", \
                   g_section_name);                                  \
    } while (0)

/* Per-test macros */
#define TEST(name)                              \
    do {                                        \
        g_tests_run++;                          \
        if (g_verbose)                          \
            printf("  [TEST] %s ... ", (name)); \
    } while (0)

#define PASS()                \
    do {                      \
        g_tests_passed++;     \
        if (g_verbose)        \
            printf("PASS\n"); \
    } while (0)

#define FAIL(msg)                                   \
    do {                                            \
        g_tests_failed++;                           \
        printf("  [FAIL] %s: %s\n", __func__, msg); \
    } while (0)

/* Assertions that report failure without aborting the suite */
#define ASSERT_TRUE(cond)            \
    do {                             \
        if (!(cond)) {               \
            FAIL(#cond " is false"); \
            return;                  \
        }                            \
    } while (0)

#define ASSERT_EQ(a, b)         \
    do {                        \
        if ((a) != (b)) {       \
            FAIL(#a " != " #b); \
            return;             \
        }                       \
    } while (0)

#define ASSERT_NE(a, b)         \
    do {                        \
        if ((a) == (b)) {       \
            FAIL(#a " == " #b); \
            return;             \
        }                       \
    } while (0)

#define ASSERT_GT(a, b)         \
    do {                        \
        if (!((a) > (b))) {     \
            FAIL(#a " <= " #b); \
            return;             \
        }                       \
    } while (0)

#define ASSERT_NOT_NULL(ptr)       \
    do {                           \
        if ((ptr) == NULL) {       \
            FAIL(#ptr " is NULL"); \
            return;                \
        }                          \
    } while (0)

/* Define counters in the translation unit that defines TEST_MAIN */
#ifdef TEST_MAIN
int g_tests_run;
int g_tests_passed;
int g_tests_failed;
int g_verbose;
int g_section_failed;
const char *g_section_name;
#endif

#endif /* TEST_COMMON_H */
