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
#include "acr.h"
#include "aim.h"
#include "log.h"

#include <unistd.h>

#define AIM_MAX_WORKERS 64

int are_main(int argc, char **argv) {

    (void) argc;
    (void) argv;

    aim_hndl *pAIM = NULL;
    acl_hndl *pACL = NULL;
    acr_hndl *pACR = NULL;

    pAIM = aim_init(AIM_MAX_WORKERS);
    pACL = acl_init(pAIM);
    pACR = acr_init(pAIM);

    aci_keepalive(true);

    while (true) {
        aim_entry_t *pInstance = aim_dequeue(pAIM);
        if (!pInstance) {
            usleep(5000);
            continue;
        }
        eACN_notification pending;
        eACN_error acn_err = acn_check(pInstance->pACN, &pending);

        if (acn_err == eACN_ERR_FATAL) {
            log_info(
                "ACN Returned Error.. "
                "Assuming client disconnected and closing the connection.");
            acr_run(pACR, pInstance, eACR_shutdowndisconnect);
        } else if (acn_err == eACN_ERR_UCS) {
            acr_run(pACR, pInstance, eACR_nop);
        } else if (pending & eACN_checkpoint) {
            log_debug("Checkpoint Pending..");
        } else if (pending & eACN_restore) {
            log_debug("Restore Pending..");
        } else {
            log_debug("Nothing Pending..");
            acr_run(pACR, pInstance, eACR_nop);
        }
    }

    aci_keepalive(false);

    acr_finalize(&pACR);
    acl_finalize(&pACL);
    aim_finalize(&pAIM);

    return 0;
}
