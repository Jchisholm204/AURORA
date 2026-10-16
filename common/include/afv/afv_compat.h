/**
 * @file afv_compat.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Compatibility Header for old metadata operations
 * @version 0.1
 * @date Created: 2026-10-15
 * @modified Last Modified: 2026-10-15
 *
 * @copyright Copyright (c) 2026
 */

#ifndef __AFV_COMPAT_H__
#define __AFV_COMPAT_H__

// Do not allow compat functions internally
#ifdef AFV_INTERNAL
#define AFV_NO_COMPAT
#endif

#ifndef AFV_NO_COMPAT

#warning "AFV Compatibilty Mode Enabled"

#define afv_get_metadata_versioned(pHndl, version, name)                       \
    afv_read_metadata(pHndl, version, name)

#define afv_get_metadata(pHndl) afv_read_metadata(pHndl, -1, NULL)

#define afv_destroy_metadata(ppHndl) afv_metadata_destroy(ppHndl)

#define afv_create_metadata(n_regions) afv_metadata_create(n_regions)

#endif

#endif
