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

#include "aci/aci.h"
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

    while (true) {
        aim_entry_t *pInstance = aim_dequeue(pAIM);
        if (!pInstance) {
            usleep(100);
            continue;
        }
        aci_poll(pInstance->pACI);
        if(aim_enqueue(pAIM, pInstance) != 0){
            log_error("AIM Enqueue Failed??");
        }
    }

    acl_finalize(&pACL);
    aim_finalize(&pAIM);

    return 0;
}
