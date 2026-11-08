/**
 * @file main.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-11-03
 * @modified Last Modified: 2026-11-03
 *
 * @copyright Copyright (c) 2026
 */

#include "main.h"

#include "ael/ael.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    printf("Hello\n");

    int port = 8335;
    ael_hndl *pAEL = ael_create_instance(eAEL_N_ORIGIN, 0, 0, 0, port, 0);

    // Recv messages

    ael_destroy_instance(&pAEL);
    return 0;
}
