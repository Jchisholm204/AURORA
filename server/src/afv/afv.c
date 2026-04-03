/**
 * @file afv.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief File Versioning System
 * @version 0.1
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-01
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL
#include "afv/afv.h"

#include "afv/afv_file.h"
#include "log.h"

#include <dirent.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#define METADATA_KEY ((uint64_t) 0x73C4D8823495423AULL)

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
    pHndl->pMetadata = NULL;

    if (!pHndl->persistent_path) {
        log_error("Bad Alloc??");
        free(pHndl);
        return NULL;
    }

    if (!pHndl->pMetadata) {
        log_info("Unable to find metadata for %d of %d in group %d at path: %s",
                 rank, group_size, group_id, persistent_path);
    }

    // Need to load metadata from the file
    pHndl->pMetadata =
        (afv_metadata_t *) afv_create_metadata(rank, 0, "______INIT_____", 0,
                                               NULL, NULL);
    if (!pHndl->pMetadata) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pHndl->rank = rank;
    pHndl->group_id = group_id;
    pHndl->use_error_correction = with_ec;

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
                memcpy(pFound_name, parsed_name, AFV_FNAME_LEN);
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

    char filename[AFV_FNAME_LEN];

    if (version < 0) {
        version = _afv_get_latest_version(pHndl, name, filename);
    } else {
        snprintf(filename, AFV_FNAME_LEN, "%s-%lu-%lu-%lu.afvmeta", name,
                 version, pHndl->rank, pHndl->group_id);
    }
    if (version < 0) {
        log_error("Metadata not found");
        return NULL;
    }

    FILE *pFile = fopen(filename, "r");
    if (!pFile) {
        log_error("Metadata not found");
        return NULL;
    }

    uint64_t metadata_reserved[AFV_RESERVED_SIZE];

    ulong read_bytes =
        fread(metadata_reserved, AFV_RESERVED_SIZE, sizeof(uint64_t), pFile);

    if (read_bytes != (AFV_RESERVED_SIZE * sizeof(uint64_t))) {
        log_error("Metadata not found");
        (void) fclose(pFile);
        return NULL;
    }

    uint64_t metadata_size = metadata_reserved[0];
    uint64_t metadata_key = metadata_reserved[1];

    afv_metadata_t *pMetadata = malloc(metadata_size);

    if (!pMetadata) {
        log_error("Bad Alloc??");
        (void) fclose(pFile);
        return NULL;
    }

    if (metadata_key != METADATA_KEY) {
        log_error("Metadata not found");
        (void) fclose(pFile);
        return NULL;
    }

    // Go to beginning of file, read all data out
    (void) fseek(pFile, 0, SEEK_SET);
    read_bytes = fread(pMetadata, 1, metadata_size, pFile);

    if (read_bytes != metadata_size) {
        log_error("Metadata not found");
        free(pMetadata);
        (void) fclose(pFile);
        return NULL;
    }

    (void) fclose(pFile);

    // Setup the metadatas internal pointer structure
    pMetadata = afv_metadata_ptr_init(pMetadata);
    return pMetadata;
}

const afv_metadata_t *afv_get_metadata(afv_hndl *pHndl) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return NULL;
    }
    return pHndl->pMetadata;
}

int afv_write_metadata(afv_hndl *pHndl, afv_metadata_t *const pMetadata) {
}

uint64_t afv_get_rank(afv_hndl *pHndl) {
}
