/**
 * @file main.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief UCX Transport Client
 * @version 0.1
 * @date Created: 2026-02-10
 * @modified Last Modified: 2026-02-10
 *
 * @copyright Copyright (c) 2026
 */

#include "transport_client.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    barf::TransportClient tl("127.0.0.1");

    const uint64_t id = 8;

    barf::TransportBase::TransportConnection tc = tl.connect();

    std::cout << "Client Connected!!";

    tl.send_cmd(tc, barf::Command(barf::Command::eCommandInit, id, 3, "Hello"));

    barf::Command cmd;
    while (!tl.dequeue_cmd(cmd)) {
        log_info("Cli %d waiting for connection...", id);
        tl.progress();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    int test_int = 0;
    barf::Region r(id, 0, &test_int, sizeof(test_int));

    tl.send_region(tc, r);

    for (int i = 0;; i++) {
        std::cout << tl.progress() << std::endl;
        std::cout << test_int << std::endl;
        test_int = i;
        tl.send_cmd(tc, barf::Command(barf::Command::eCommandNone, id, i,
                                      "Keep Alive"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
