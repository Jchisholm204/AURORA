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
#define ACR_MAX_WORKERS 2
#endif

int are_main(int argc, char **argv) {
    FILE *pLogFile = NULL;
    if (argc >= 2) {
        pLogFile = fopen(argv[1], "w");
        if (pLogFile && argc == 2) {
            (void) log_add_fp(pLogFile, LOG_TRACE);
            log_set_level(LOG_DEBUG);
        } else if (pLogFile && argc == 3) {
            log_set_level(LOG_DEBUG);
            int log_level = atoi(argv[2]);
            if (log_level > 0 && log_level < LOG_FATAL) {
                (void) log_add_fp(pLogFile, log_level);
            } else {
                (void) log_add_fp(pLogFile, LOG_TRACE);
            }
        } else {
            log_error("Log File Failure");
        }
    }

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
            if (pInstance->error_counter <= (ARE_MAX_ERROR_COUNT + 2)) {
                log_debug("ACN Returned Fatal Error.. Closing Connection");
            }
            eACR_error acr_status =
                acr_run(pACR, pInstance, 0, acr_cmd_connection_down);
            if (acr_status != eACR_OK) {
                acr_run(pACR, pInstance, 0, acr_cmd_nop);
                pInstance->error_counter++;
            }
        } else if (acn_err == eACN_ERR_UCS) {
            log_trace("UCS Error");
            pInstance->error_counter++;
            acr_run(pACR, pInstance, 0, acr_cmd_nop);
        } else if (acn_err == eACN_ERR_TIMEOUT) {
            acr_run(pACR, pInstance, 0, acr_cmd_nop);
        } else if (pending & eACN_checkpoint) {
            eACR_error acr_status =
                acr_run(pACR, pInstance, 0, acr_cmd_checkpoint);
            if (acr_status != eACR_OK) {
                acr_run(pACR, pInstance, 0, acr_cmd_nop);
            }
        } else if (pending & eACN_restore) {
            eACR_error acr_status =
                acr_run(pACR, pInstance, 0, acr_cmd_restart);
            if (acr_status != eACR_OK) {
                acr_run(pACR, pInstance, 0, acr_cmd_nop);
            }
        } else {
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

    if (pLogFile) {
        fclose(pLogFile);
        pLogFile = NULL;
    }

    return 0;
}
