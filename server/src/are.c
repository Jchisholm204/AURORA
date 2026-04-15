/**
 * @file are.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-02-27
 * @modified Last Modified: 2026-03-13
 *
 * @copyright Copyright (c) 2026
 */

#include "are.h"

#include "acl.h"
#include "acr/acr.h"
#include "aim.h"
#include "log.h"

#include <unistd.h>

#ifndef ARE_MAX_ERROR_COUNT
#define ARE_MAX_ERROR_COUNT 16
#endif

#ifndef AIM_MAX_WORKERS
#define AIM_MAX_WORKERS 64
#endif
#ifndef ACR_MAX_WORKERS
#define ACR_MAX_WORKERS 4
#endif

int are_main(int argc, char **argv) {

    (void) argc;
    (void) argv;

    aim_hndl *pAIM = NULL;
    acl_hndl *pACL = NULL;
    acr_hndl *pACR = NULL;

    pAIM = aim_init(AIM_MAX_WORKERS);
    pACR = acr_init(pAIM, ACR_MAX_WORKERS);
    pACL = acl_init(pAIM, pACR);

    aci_keepalive(true);

    while (true) {
        aim_entry_t *pInstance = aim_dequeue(pAIM);
        if (!pInstance) {
            usleep(1000);
            continue;
        }
        eACN_notification pending;
        eACN_error acn_err = acn_check(pInstance->pACN, &pending);

        if (acn_err == eACN_ERR_FATAL ||
            pInstance->error_counter > ARE_MAX_ERROR_COUNT) {
            log_info("ACN Returned Fatal Error.. Closing Connection");
            eACR_error acr_status =
                acr_run(pACR, pInstance, 0, acr_cmd_connection_down);
            if (acr_status != eACR_OK) {
                acr_run(pACR, pInstance, 0, acr_cmd_nop);
            }
        } else if (acn_err == eACN_ERR_UCS) {
            log_error("UCS Error");
            pInstance->error_counter++;
            acr_run(pACR, pInstance, 0, acr_cmd_nop);
        } 
        else if (acn_err == eACN_ERR_TIMEOUT) {
            acr_run(pACR, pInstance, 0, acr_cmd_nop);
        } 
        else if (pending & eACN_checkpoint) {
            eACR_error acr_status =
                acr_run(pACR, pInstance, 0, acr_cmd_checkpoint);
            if (acr_status != eACR_OK) {
                acr_run(pACR, pInstance, 0, acr_cmd_nop);
            }
        }
        else if (pending & eACN_restore) {
            eACR_error acr_status =
                acr_run(pACR, pInstance, 0, acr_cmd_restart);
            if (acr_status != eACR_OK) {
                acr_run(pACR, pInstance, 0, acr_cmd_nop);
            }
        } 
        else {
            eACR_error acr_status = acr_run(pACR, pInstance, 0, acr_cmd_nop);
            if (acr_status != eACR_OK) {
                acr_run(pACR, pInstance, 0, acr_cmd_nop);
            }
        }
    }

    aci_keepalive(false);

    acr_finalize(&pACR);
    acl_finalize(&pACL);
    aim_finalize(&pAIM);

    return 0;
}
