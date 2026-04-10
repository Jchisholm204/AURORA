/**
 * @file test_discovery.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#include "aul.h"

#include <memory.h>
#include <stdio.h>

#define MEM_SIZE 4

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    aul_configuration_t cfg = AUL_CONFIG_DEFAULT;
    int ini_s = AUL_Init(&cfg);

    printf("AUL Initialized %d\n", ini_s);

    int test = AUL_Test(-1, "TestCkpt0000001");
    printf("Latest = %d (invalid)\n", test);

    uint64_t buf[MEM_SIZE] = {0xBEEF, 0xDEAD, 0xC0FF1E, 0xBEEF};
    uint64_t bu2[MEM_SIZE];
    memcpy(bu2, buf, sizeof(buf));

    AUL_Mem_protect(3, buf, sizeof(buf));

    test = AUL_Test(-1, "TestCkpt0000001");
    printf("Latest = %d (invalid)\n", test);

    AUL_Mem_protect(5, buf, sizeof(buf));
    AUL_Mem_protect(7, buf, sizeof(buf));

    test = AUL_Test(-1, "TestCkpt0000001");
    printf("Latest = %d (valid)\n", test);
    test = AUL_Test(2, "TestCkpt0000001");
    printf("v=2 = %d (invalid)\n", test);
    test = AUL_Test(-1, "Tggt5krl0j00001");
    printf("invalid name = %d (invalid)\n", test);
    test = AUL_Test(2, NULL);
    printf("null name = %d (invalid)\n", test);
    test = AUL_Test(-1, NULL);
    printf("latest null = %d (valid)\n", test);

    printf("Starting Checkpoint\n");
    // *nowarn*
    int s = AUL_Checkpoint(1, "TestCkpt0000001");
    printf("Finished Checkpoint status=%d\n", s);

    for (int i = 0; i < MEM_SIZE; i++) {
        buf[i] = 0;
    }

    // s = AUL_Checkpoint(2, "TestCkpt0000002");
    // printf("Finished Checkpoint status=%d\n", s);

    printf("Starting Restore\n");
    // *nowarn*
    int v = AUL_Restart(1, "TestCkpt0000001");
    printf("Finished Restore latest version=%d\n", v);

    for (int i = 0; i < MEM_SIZE; i++) {
        if (buf[i] != bu2[i]) {
            printf("Error: 0x%lx != 0x%lx\n", buf[i], bu2[i]);
        }
    }

    AUL_Finalize();

    printf("AUL Finalized\n");

    return 0;
}
