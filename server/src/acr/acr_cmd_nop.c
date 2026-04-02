/**
 * @file acr_cmd_nop.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief NOP Command for AURORA Command Runner
 * @version 0.1
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define ACR_INTERNAL
#include "acr/acr.h"
#include "aim.h"
#include "log.h"

void *acr_cmd_nop(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // Do the command stuff

    // NOP
    // log_trace("NOP");

    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    return NULL;
}
