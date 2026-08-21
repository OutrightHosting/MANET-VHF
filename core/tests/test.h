/*
 * Minimal test harness. Tests live outside core/src and may use stdio; the core
 * itself may not (ADR-0006).
 */
#ifndef MANET_TEST_H
#define MANET_TEST_H

#include <stdio.h>

extern int manet_test_failures;
extern int manet_test_checks;

#define CHECK(cond)                                                              \
    do {                                                                         \
        manet_test_checks++;                                                     \
        if (!(cond)) {                                                           \
            manet_test_failures++;                                               \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        const long _a = (long)(a);                                               \
        const long _b = (long)(b);                                               \
        manet_test_checks++;                                                     \
        if (_a != _b) {                                                          \
            manet_test_failures++;                                               \
            printf("  FAIL %s:%d  %s == %s  (%ld != %ld)\n",                     \
                   __FILE__, __LINE__, #a, #b, _a, _b);                          \
        }                                                                        \
    } while (0)

void test_addr_all(void);
void test_frame_all(void);
void test_config_all(void);
void test_slot_all(void);
void test_mesh_all(void);
void test_dedup_all(void);

#endif /* MANET_TEST_H */
