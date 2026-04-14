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
#include "ads/ads.h"
#include "aim.h"
#include "log.h"
#include "operating_configuration.h"

void *acr_cmd_connection_up(void *arg) {
    struct aurora_command_ctx *pCtx = arg;
    if (!pCtx) {
        log_error("NULL Parameter");
        return NULL;
    }

    log_trace("Attempting to accept new connection");

    aim_entry_t *pCli = aim_add_entry(pCtx->pAIM);
    if (!pCli) {
        log_error("Failed to retrieve memory for new AIM entry");
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }
    ads_exchange_data_t ads_data_tx = {0};
    ads_exchange_data_t *ads_data_rx = NULL;

    pCli->pACI = aci_create_instance(&ads_data_tx.comm);
    pCli->pACN = acn_create_instance(pCli->pACI, &ads_data_tx.notif);

    if (!pCli->pACI || !pCli->pACN) {
        log_error("Instance Failure");
        acn_destroy_instance(&pCli->pACN);
        aci_destroy_instance(&pCli->pACI);
        aim_remove_entry(pCtx->pAIM, pCli);
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }

    ads_data_rx = ads_exchange(pCtx->flags, &ads_data_tx);

    if (!ads_data_rx) {
        log_error("ADS Exchange failure.");
        acn_destroy_instance(&pCli->pACN);
        aci_destroy_instance(&pCli->pACI);
        aim_remove_entry(pCtx->pAIM, pCli);
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }

    opconf_t *pConfig = ads_data_rx->config.data;

    if (!pConfig || ads_data_rx->config.size == sizeof(opconf_t)) {
        log_error("No/Invalid Configuration??");
        acn_destroy_instance(&pCli->pACN);
        aci_destroy_instance(&pCli->pACI);
        pConfig = NULL;
        aim_remove_entry(pCtx->pAIM, pCli);
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }

    else {
        pConfig->chkpt_opts.persistent_path =
            (char *) ((uint8_t *) pConfig) + sizeof(opconf_t);

        log_debug("rank=%d", pConfig->rank);
        log_debug("group_id=%d", pConfig->group.id);
        log_debug("group_size=%d", pConfig->group.size);
        log_debug("path=%s", pConfig->chkpt_opts.persistent_path);
    }

    int aci_status = 0;
    aci_status =
        aci_connect_instance(pCli->pACI, &ads_data_tx.comm, &ads_data_rx->comm);
    if (aci_status != 0) {
        log_error("Instance Failure");
        acn_destroy_instance(&pCli->pACN);
        aci_destroy_instance(&pCli->pACI);
        aim_remove_entry(pCtx->pAIM, pCli);
        aoc_free((opconf_t **) &ads_data_rx->config.data);
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }
    eACN_error acn_status = eACN_OK;
    acn_status = acn_connect_instance(pCli->pACN, &ads_data_tx.notif,
                                      &ads_data_rx->notif);
    if (acn_status != eACN_OK) {
        log_error("Instance Failure");
        acn_destroy_instance(&pCli->pACN);
        aci_destroy_instance(&pCli->pACI);
        aim_remove_entry(pCtx->pAIM, pCli);
        aoc_free((opconf_t **) &ads_data_rx->config.data);
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }

    // ARM must be created after ACI/ACN
    pCli->pARM = arm_create_instance(pCli->pACI, pCli->pACN);

    if (!pCli->pARM) {
        log_error("Instance Failure");
        acn_destroy_instance(&pCli->pACN);
        aci_destroy_instance(&pCli->pACI);
        aim_remove_entry(pCtx->pAIM, pCli);
        aoc_free((opconf_t **) &ads_data_rx->config.data);
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }

    pCli->pAFV = afv_create_instance(pConfig->rank, pConfig->group.id,
                                     pConfig->group.size,
                                     pConfig->chkpt_opts.persistent_path,
                                     pConfig->chkpt_opts.use_error_correction);

    if (!pCli->pAFV) {
        log_error("Instance Failure");
        arm_destroy_instance(&pCli->pARM);
        acn_destroy_instance(&pCli->pACN);
        aci_destroy_instance(&pCli->pACI);
        aim_remove_entry(pCtx->pAIM, pCli);
        aoc_free((opconf_t **) &ads_data_rx->config.data);
        (void) _acr_ctx_release_retry(pCtx, 2);
        return NULL;
    }

    // Free the configuration chunk
    aoc_free((opconf_t **) &ads_data_rx->config.data);

    if (ads_data_rx) {
        free(ads_data_rx);
        ads_data_rx = NULL;
    }

    log_trace("New connection accepted.");
    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCli) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;

    // Release the thread context
    (void) _acr_ctx_release_retry(pCtx, 2);

    return NULL;
}

void *acr_cmd_connection_down(void *arg) {
    if (!arg) {
        log_fatal("NULL Parameter");
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

    // Release the thread context
    (void) _acr_ctx_release_retry(pCtx, 2);
    return NULL;
}
