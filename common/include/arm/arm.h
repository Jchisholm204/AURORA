/**
 * @file arm.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Region Manager
 * @version 0.1
 * @date Created: 2026-03-19
 * @modified Last Modified: 2026-03-19
 *
 * This code CANNOT be called from multiple threads
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ARM_ARM_H_
#define _ARM_ARM_H_

#ifndef ARM_NAME_LEN
#define ARM_NAME_LEN 16
#endif

#include "aci/aci.h"

#include <stdint.h>

#ifdef ARM_INTERNAL
#include <ucp/api/ucp.h>
// ARM DMA Threshold (64KB)
#define ARM_DMA_THRESH (64 * 1024)
// Default number of instances
#define ARM_INIT_INSTANCES 16
#endif

struct aurora_memory_region_hndl {
    // Store as uint64_t to prevent prefetching
    uint64_t pActive_memory;
    uint64_t pShadow_memory;
    const size_t rgn_size;
    const uint64_t id;
    const char name[ARM_NAME_LEN];
    union {
#ifdef ARM_INTERNAL
        struct {
            ucp_rkey_h remote_key;
            ucp_mem_h mem_hndl;
        };
#endif
        uint64_t __reserved[2];
    };
};

struct aurora_region_manager_hndl
#ifdef ARM_INTERNAL
{
    // ACI Handle
    aci_hndl *pACI;
    struct aurora_memory_region_hndl *regions;
    size_t n_regions;
}
#endif
;

enum aurora_region_manager_error_e {
    eARM_OK = 0,
    eARM_ERR_ID_NOT_FOUND,
    eARM_ERR_NAME_NOT_FOUND,
    eARM_ERR_MATCH_NOT_FOUND,
    eARM_ERR_NULL,
    eARM_ERR_INPROGRESS,
    eARM_ERR_UCS,
    eARM_ERR_FATAL,
    eARM_N_ERR,
};

typedef struct aurora_memory_region_hndl amr_hndl;
typedef struct aurora_region_manager_hndl arm_hndl;
typedef enum aurora_region_manager_error_e eARM_error;

/**
 * @brief Create an ARM instance
 *
 * @param pACI The Connection Instance to attach to
 * @return
 */
extern arm_hndl *arm_create_instance(aci_hndl *pACI);

/**
 * @brief Destroy an ARM instance
 *
 * @param ppHndl ARM handle to destroy
 * @return
 */
extern eARM_error arm_destroy_instance(arm_hndl **ppHndl);

/**
 * @brief Add/Setup a ARM Region
 *
 * @param pHndl ARM Handle to add to
 * @param pAMR Region (with user fields filled out)
 * @return
 */
extern eARM_error arm_add(arm_hndl *pHndl, const amr_hndl *pAMR);

/**
 * @brief Remove/Deinit an ARM Region
 *
 * @param pHndl ARM Handle to remove from
 * @param pAMR Match Criteria (Will remove any regions that match the values in
 * this struct)
 * @return
 */
extern eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR);

/**
 * @brief Get the number of regions within the ARM
 *
 * @param pHndl ARM Handle
 * @return The number of regions associated with the handle
 */
extern size_t arm_get_n_regions(arm_hndl *pHndl);

/**
 * @brief Get read only access to a handles regions
 *
 * @param pHndl Handle to access
 * @return read only pointer to the internal ARM array
 */
extern const amr_hndl *arm_get_regions(arm_hndl *pHndl);

/**
 * @brief RDMA Write to a region (shadow or active)
 *
 * @param pHndl ARM Handle the region belongs to
 * @param pAMR Memory Region Handle
 * @param remote_addr Remote address to write to
 * @param data Local data to write
 * @param size size of the write
 * @return Err or OK
 */
extern eARM_error arm_write(arm_hndl *pHndl, const amr_hndl *pAMR,
                            const uint64_t remote_addr, const void *data,
                            size_t size);

/**
 * @brief RDMA Read from a region (shadow or active)
 *
 * @param pHndl ARM Handle the region belongs to
 * @param pAMR Memory Region Handle
 * @param remote_addr Remote address to read from
 * @param data region to copy the read data to
 * @param size size of the transfer
 * @return Err or OK
 */
extern eARM_error arm_read(arm_hndl *pHndl, const amr_hndl *pAMR,
                           const uint64_t remote_addr, void *data, size_t size);

/**
 * @brief Specialized Copy/Sync function to copy active memory into the shadow
 * memory
 *
 * @param pHndl ARM Handle
 * @param pAMR AMR to execute the sync on
 * @return Err or OK
 */
extern eARM_error delllsync(arm_hndl *pHndl, const amr_hndl *pAMR);

#endif
