/**
 * @file savemem.cpp
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-19
 * @modified Last Modified: 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "client.hpp"

#include <chrono>
#include <iostream>

#define BUF_N 5

int main(int argc, char **argv) {
    uint64_t rank = 0;
    if (argc == 2) {
        rank = atoi(argv[1]);
    }

    // Init the Client
    barf::Client cli(rank);

    // Declare some memory
    char *buffer = (char *) malloc(BUF_N);
    int i = 0, j = 0;

    cli.mem_protect(0, &i, sizeof(i));
    cli.mem_protect(1, &j, sizeof(j));
    cli.mem_protect(2, buffer, BUF_N);

    // -1 for most recent
    if (cli.restart(20, "savemem")) {
        std::cout << "Program Restarting from " << i << std::endl;
    }

    for (;; i++) {
        for (j = 0; j < BUF_N; j++) {
            buffer[j] = i;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (i % 10 == 0 && i != 0) {
            std::cout << "Starting Checkpoint: " << i / 10 << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            cli.checkpoint(i / 10, "savemem");
            auto end = std::chrono::high_resolution_clock::now();
            std::cout << "Checkpointing took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             end - start)
                             .count()
                      << "ms" << std::endl;
        }
        std::cout << "Iteration " << i << std::endl;
    }

    return 0;
}
