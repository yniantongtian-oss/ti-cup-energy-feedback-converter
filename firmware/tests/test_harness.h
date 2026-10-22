#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <math.h>
#include <stdio.h>

/* assert() disappears under the Release flags CI uses (-DNDEBUG). These checks
 * always run and keep going so one failure does not hide the next.
 */
static int g_test_failures = 0;

#define CHECK(condition)                                                                 \
    do {                                                                                 \
        if (!(condition)) {                                                              \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,          \
                    #condition);                                                         \
            g_test_failures += 1;                                                        \
        }                                                                                \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                          \
    CHECK(fabsf((actual) - (expected)) <= (tolerance))

#endif
