/**
 * @file acr_cmd_connection.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Connection Commands (part of the AURORA Command Runner)
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

void *acr_cmd_connection_down(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    log_trace("ACR is intentionally destroying an instance");

    // Run the command stuffs
    arm_destroy_instance(&pCtx->pInstance->pARM);
    acn_destroy_instance(&pCtx->pInstance->pACN);
    aci_disconnect_instance(pCtx->pInstance->pACI);
    aci_destroy_instance(&pCtx->pInstance->pACI);
    afv_destroy_instance(&pCtx->pInstance->pAFV);

    // Deal with AIM
    if (aim_remove_entry(pCtx->pAIM, pCtx->pInstance)) {
        log_error("Failed to remove AIM entry");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    return NULL;
}
