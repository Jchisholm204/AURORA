/**
 * @file acr.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Command Runner
 * @version 0.1
 * @date Created: 2026-03-12
 * @modified Last Modified: 2026-03-12
 *
 * @copyright Copyright (c) 2026
 */

#include "aim.h"

#define MAX_WORKERS 16

#ifdef ACR_INTERNAL
struct aurora_command_ctx {
    aim_hndl *pAIM;
    aim_entry_t *pInstance;
};
#endif

struct aurora_command_runner_hndl
#ifdef ACR_INTERNAL
{
    atomic_int refcount;
    aim_hndl *pAIM;
    struct aurora_command_ctx thread_contexts[MAX_WORKERS];
}
#endif
;

enum aurora_command_runner_cmds_e {
    eACR_nop,
    eACR_checkpoint,
    eACR_restore,
    eACR_shutdowndisconnect,
    eACR_N,
};

typedef enum aurora_command_runner_cmds_e eACR_cmd;
typedef struct aurora_command_runner_hndl acr_hndl;

extern acr_hndl *acr_init(aim_hndl *pAIM);

extern int acr_finalize(acr_hndl **ppHndl);

extern int acr_run(acr_hndl *pHndl, aim_entry_t *pInstance, eACR_cmd ecmd);
