/**
 * @file afv.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief File Versioning System
 * @version 0.2
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL
#include "afv/afv.h"

#include "afv/afv_file.h"
#include "log.h"

#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

afv_hndl *afv_create_instance(uint64_t rank, uint64_t group_id,
                              int64_t group_size, char *persistent_path,
                              bool with_ec) {
    if (!persistent_path) {
        log_error("NULL Parameter");
        return NULL;
    }

    afv_hndl *pHndl = malloc(sizeof(afv_hndl));
    if (!pHndl) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pHndl->persistent_path = strdup(persistent_path);
    pHndl->rank = rank;
    pHndl->group_id = group_id;
    pHndl->use_error_correction = with_ec;
    pHndl->pMetadata = NULL;

    if (!pHndl->persistent_path) {
        log_error("Bad Alloc??");
        free(pHndl);
        return NULL;
    }

    // Check validity of the path
    DIR *dir = opendir(pHndl->persistent_path);
    if (dir) {
        closedir(dir);
    } else if (errno == ENOENT) {
        log_fatal("Persistent Path Not Found");
        free(pHndl->persistent_path);
        free(pHndl);
        return NULL;
    } else if (errno == EACCES) {
        log_fatal("Persistent Path Access Denied");
        free(pHndl->persistent_path);
        free(pHndl);
        return NULL;
    }

    // Need to load metadata from the file
    pHndl->pMetadata =
        (afv_metadata_t *) afv_get_metadata_versioned(pHndl, -1, NULL);

    if (!pHndl->pMetadata) {
        log_info("Unable to find metadata for %d of %d in group %d at path: %s",
                 rank, group_size, group_id, persistent_path);
        pHndl->pMetadata = afv_create_metadata(0);
    }

    if (!pHndl->pMetadata) {
        log_error("Bad Alloc??");
        free(pHndl->persistent_path);
        free(pHndl);
        return NULL;
    }

    return pHndl;
}

void afv_destroy_instance(afv_hndl **ppHndl) {
    if (!ppHndl) {
        return;
    }
    afv_hndl *pHndl = *ppHndl;
    if (!pHndl) {
        return;
    }
    if (pHndl->persistent_path) {
        free(pHndl->persistent_path);
    }
    if (pHndl->pMetadata) {
        afv_destroy_metadata(&pHndl->pMetadata);
    }
    free(pHndl);
    *ppHndl = NULL;
}

int64_t _afv_get_latest_version(afv_hndl *pHndl, const char *name,
                                char *pFound_name) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return -1;
    }

    DIR *pDir = opendir(pHndl->persistent_path);
    if (!pDir) {
        log_error("Failed to open dir: %s", pHndl->persistent_path);
        return -2;
    }

    int64_t latest_version = -1;

    // Target metadata extension
    static const char *ext_suffix = ".afvmeta";
    size_t ext_len = strlen(ext_suffix);

    struct dirent *pEntry = NULL;
    while ((pEntry = readdir(pDir)) != NULL) {
        // Suffix Compare
        size_t name_len = strlen(pEntry->d_name);
        if (name_len <= ext_len ||
            strcmp(pEntry->d_name + name_len - ext_len, ext_suffix) != 0) {
            continue;
        }

        char parsed_name[AFV_FNAME_LEN] = "";
        uint64_t parsed_ver = 0;
        uint64_t parsed_rank = 0;
        uint64_t parsed_gid = 0;

        int matched =
            sscanf(pEntry->d_name, "%[^-]-%lu-%lu-%lu.afvmeta", parsed_name,
                   &parsed_ver, &parsed_rank, &parsed_gid);

        bool found = true;
        found &= (matched == 4);
        found &= (name == NULL || strcmp(name, parsed_name) == 0);
        found &= (parsed_rank == pHndl->rank);
        found &=
            (parsed_gid == (uint64_t) pHndl->group_id) || (pHndl->group_id < 0);

        if (found && (int64_t) parsed_ver > latest_version) {
            latest_version = (int64_t) parsed_ver;
            if (pFound_name)
                snprintf(pFound_name, AFV_FNAME_LEN, "%s/%s",
                         pHndl->persistent_path, pEntry->d_name);
        }
    }
    pEntry = NULL;

    closedir(pDir);

    // Returns -1 if no valid checkpoint files were found
    return latest_version;
}

const afv_metadata_t *afv_get_metadata_versioned(afv_hndl *pHndl,
                                                 int64_t version,
                                                 const char *name) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return NULL;
    }

    char filename[AFV_FNAME_LEN] = "FILENOTFOUND";

    if (version < 0) {
        version = _afv_get_latest_version(pHndl, name, filename);
    } else {
        snprintf(filename, AFV_FNAME_LEN, "%s/%.*s-%lu-%lu-%lu.afvmeta",
                 pHndl->persistent_path, AFV_CKPT_NAME_LEN, name, version,
                 pHndl->rank, pHndl->group_id);
    }

    log_trace("Lookup: %s", filename);

    if (version < 0) {
        log_warn("Metadata not found");
        return NULL;
    }

    FILE *pFile = fopen(filename, "r");
    if (!pFile) {
        log_warn("File Error");
        return NULL;
    }

    uint64_t metadata_reserved[AFV_RESERVED_SIZE];

    ulong read_bytes = fread(metadata_reserved, 1,
                             AFV_RESERVED_SIZE * sizeof(uint64_t), pFile);

    if (read_bytes != (AFV_RESERVED_SIZE * sizeof(uint64_t))) {
        log_error("File Error %d", read_bytes);
        (void) fclose(pFile);
        return NULL;
    }

    uint64_t metadata_size = afv_metadata_ptr_size(metadata_reserved);

    afv_metadata_t *pMetadata = malloc(metadata_size);

    if (!pMetadata) {
        log_error("Bad Alloc??");
        (void) fclose(pFile);
        return NULL;
    }

    // Go to beginning of file, read all data out
    (void) fseek(pFile, 0, SEEK_SET);
    read_bytes = fread(pMetadata, 1, metadata_size, pFile);

    if (read_bytes != metadata_size) {
        log_error("File Error");
        free(pMetadata);
        (void) fclose(pFile);
        return NULL;
    }

    fclose(pFile);

    // Setup the metadatas internal pointer structure
    pMetadata = afv_metadata_ptr_init(pMetadata);
    eAFV_verif verif_status = afv_metadata_verify(pMetadata);
    if (verif_status != eAFV_VERIF_OK) {
        log_warn("Verification Failed=0x%lx", verif_status);
        free(pMetadata);
        return NULL;
    }

    log_trace("Loaded Metadata: %s %d", pMetadata->chkpt_name,
              pMetadata->version);
    for (size_t i = 0; i < pMetadata->n_regions; i++) {
        log_trace(" -> %d) %s (%d) size=%d", i, pMetadata->region_names[i],
                  pMetadata->region_ids[i], pMetadata->region_sizes[i]);
    }

    return pMetadata;
}

const afv_metadata_t *afv_get_metadata(afv_hndl *pHndl) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return NULL;
    }

    return pHndl->pMetadata;
}

eAFV_verif afv_write_metadata(afv_hndl *pHndl,
                              afv_metadata_t *const pMetadata) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    if (!pMetadata) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    log_trace("Wrote Latest Metadata: %s %d", pMetadata->chkpt_name,
              pMetadata->version);
    for (size_t i = 0; i < pMetadata->n_regions; i++) {
        log_trace(" -> %d) %s (%d) size=%d", i, pMetadata->region_names[i],
                  pMetadata->region_ids[i], pMetadata->region_sizes[i]);
    }

    uint64_t group_id = 0;
    if (pHndl->group_id > 0) {
        group_id = pHndl->group_id;
    }

    char filename[AFV_FNAME_LEN];
    snprintf(filename, AFV_FNAME_LEN, "%s/%.*s-%lu-%lu-%lu.afvmeta",
             pHndl->persistent_path, AFV_CKPT_NAME_LEN, pMetadata->chkpt_name,
             pMetadata->version, pHndl->rank, group_id);

    // Verify Metadata

    eAFV_verif verif_status = afv_metadata_verify(pMetadata);
    if (verif_status != eAFV_VERIF_OK) {
        log_error("Verification Failed=0x%lx", verif_status);
        return verif_status;
    }
    if (verif_status & eAFV_VERIF_ERR_KEY) {
        pMetadata->metadata_key = AFV_METADATA_VERIF_KEY;
    }

    FILE *pFile = fopen(filename, "w");
    if (!pFile) {
        log_error("File Error");
        return eAFV_VERIF_ERR_NULL;
    }

    ulong bytes_expected = afv_metadata_ptr_size(pMetadata);
    ulong bytes_written =
        fwrite(pMetadata, 1, afv_metadata_ptr_size(pMetadata), pFile);

    if (bytes_expected != bytes_written) {
        log_error("File Error");
        (void) fclose(pFile);
        return eAFV_VERIF_ERR_NULL;
    }

    fclose(pFile);

    if (pHndl->pMetadata) {
        afv_destroy_metadata(&pHndl->pMetadata);
    }
    pHndl->pMetadata = pMetadata;

    return eAFV_VERIF_OK;
}

uint64_t afv_get_rank(afv_hndl *pHndl) {
    if (!pHndl) {
        return 0;
    }
    return pHndl->rank;
}

#ifndef FARUE
#error "FARUE was not found"
#endif
