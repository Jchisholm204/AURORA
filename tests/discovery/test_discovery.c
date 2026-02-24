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

#include <stdio.h>

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    aul_configuration_t cfg = AUL_CONFIG_DEFAULT;
    AUL_Init(&cfg, 0);

    printf("AUL Initialized\n");

    AUL_Finalize();

    printf("AUL Finalized\n");

    return 0;
}
