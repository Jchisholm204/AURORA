/**
 * @file are.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-27
 * @modified Last Modified: 2026-02-27
 *
 * @copyright Copyright (c) 2026
 */

#include "are.h"

#include "acl.h"
#include "aim.h"
#include "log.h"

#include <unistd.h>

#define AIM_MAX_WORKERS 64

int are_main(int argc, char **argv) {

    (void) argc;
    (void) argv;

    aim_hndl *pAIM = NULL;
    acl_hndl *pACL = NULL;

    pAIM = aim_init(AIM_MAX_WORKERS);
    pACL = acl_init(pAIM);

    aci_keepalive(true);

    while (true) {
        aim_entry_t *pInstance = aim_dequeue(pAIM);
        if (!pInstance) {
            usleep(5000);
            continue;
        }
        eACN_notification pending;
        int acn_err = acn_check(pInstance->pACN, &pending);
        if (acn_err != 0) {
            log_error("ACN Returned Error.. Client Disconnected?");
            log_info("Forcibly destroying client..");
            acn_destroy_instance(&pInstance->pACN);
            aci_disconnect_instance(pInstance->pACI);
            aci_destroy_instance(&pInstance->pACI);
            aim_remove_entry(pAIM, pInstance);
        }

        if (pending & eACN_checkpoint) {
            log_info("Checkpoint Pending..");
        }

        else if (pending & eACN_restore) {
            log_info("Restore Pending..");
        }

        else {
            log_info("Nothing Pending..");
        }

        if (aim_enqueue(pAIM, pInstance) != 0) {
            log_error("AIM Enqueue Failed??");
        }
    }


    aci_keepalive(false);
    acl_finalize(&pACL);
    aim_finalize(&pAIM);

    return 0;
}
