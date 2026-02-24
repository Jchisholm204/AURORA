/**
 * @file aul.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#include "aul.h"

#include "log.h"
#warning "Unimplemented"

int AUL_Init(const aul_configuration_t *pCFG, const uint64_t proc_id) {
    log_trace("Initializing AUL:");
    log_trace("\tSAVE: %s", pCFG->persistent_path);
    log_trace("\tEC: %d", pCFG->use_error_correction);
    log_trace("\tCon Mode: %d", pCFG->connection_mode);
    log_trace("\tOpt IP: %s", pCFG->opt_ip);
    log_trace("\tLog File: %s", pCFG->opt_log_file);
    log_trace("\tPID: %d", proc_id);
    return 0;
}

int AUL_Finalize(void) {
    return -1;
}

int AUL_Mem_protect(const uint64_t mem_id, const void *const ptr,
                    const size_t size) {
    (void) mem_id;
    (void) ptr;
    (void) size;
    return -1;
}

int AUL_Mem_unprotect(const uint64_t mem_id) {
    (void)mem_id;
    return -1;
}

int AUL_Checkpoint(const int version, char *name) {
    (void)version;
    (void)name;
    return -1;
}

int AUL_Restart(const int version, char *name) {
    (void)version;
    (void)name;
    return -1;
}
