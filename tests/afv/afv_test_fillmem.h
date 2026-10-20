/**
 * @file afv_test_fillmem.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-10-20
 * @modified Last Modified: 2026-10-20
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AFV_TEST_FILLMEM_H_
#define _AFV_TEST_FILLMEM_H_

#include <assert.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>

// Arbitrary ID Generation
#define RGN_ID(i) ((i + 1) * 2)

#define MEM_PATTERN(i, seed) ((i & 0xC3) * (seed + 2 * i))

static inline void test_afv_fillmem(void *mem, size_t size, size_t seed) {
    if (!mem) {
        return;
    }
    for (size_t i = 0; i < size; i++) {
        ((uint8_t *) mem)[i] = (uint8_t) (MEM_PATTERN(i, seed) & 0xFF);
    }
}

static inline void test_afv_cleanmem(void *mem, size_t size) {
    if (!mem) {
        return;
    }
    memset(mem, 0, size);
}

static inline void test_afv_testmem(void *mem, size_t size, size_t seed) {
    if (!mem) {
        return;
    }
    for (size_t i = 0; i < size; i++) {
        assert((((uint8_t *) mem)[i]) ==
               ((uint8_t) (MEM_PATTERN(i, seed) & 0xFF)));
    }
}

#endif
