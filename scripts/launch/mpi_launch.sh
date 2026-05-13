#!/usr/bin/env zsh

# Sourced Wrapper Script for submitting sbatch jobs ('launch.batch')


function ath_launch_test_mpi(){
    # Helper function to launch a slurm test (impure)
    local ATH_JOB_NAME=$1
    local ATH_JOB_TIME=$2
    local ATH_JOB_LOG=$3
    local ATH_BACKEND_ARCH=$4
    local ATH_BACKEND_NODES=$5
    local ATH_BACKEND_EXE=$6
    local ATH_TEST_ARCH=$7
    local ATH_TEST_NODES=$8
    local ATH_TEST_EXE=$9
    local ATH_TEST_PROCS=$10
    local ATH_CHECKPOINT_DIR=$11

    if [[ ! $ATH_TEST_PROCS ]]; then
        ATH_TEST_PROCS='1'
        printf "WARNING: 'nprocs' not specified. default=${ATH_TEST_PROCS}" >& 2
    fi

    mkdir -p $ATH_CHECKPOINT_DIR
    if [[ $! != 0 ]]; then
        printf "ERROR: Could not create checkpoint directory." >& 2
        exit 1
    fi

    export ATH_JOB_NODE_LIST="${ATH_BACKEND_NODES},${ATH_TEST_NODES}"
    local NODE_LIST=( ${(s:,:)ATH_JOB_NODE_LIST} )
    export ATH_JOB_NODE_COUNT=${#NODE_LIST}

    local OUT_DIR="${AURORA_LOG_DIR}/${ATH_JOB_NAME}"
    mkdir -p "$OUT_DIR"
    if [[ $? -ne 0 ]]; then
        printf "WARNING: Could not create output directory" >& 2
    fi

    local EXPORT_LIST=(
        # Testing Harness Exports
        "ATH_JOB_NAME"
        "ATH_JOB_TIME"
        "ATH_JOB_LOG"
        "ATH_BACKEND_ARCH"
        "ATH_BACKEND_NODES"
        "ATH_BACKEND_EXE"
        "ATH_TEST_ARCH"
        "ATH_TEST_NODES"
        "ATH_TEST_EXE"
        "ATH_TEST_PROCS"
        "ATH_CHECKPOINT_DIR"
        # General/Directory Exports
        "AURORA_CLUSTER_DIR"
        # Slurm Exports
        "SBATCH_PARTITION"
        "SLURM_PARTITION"
    )

    for EXPORT in $EXPORT_LIST; do
        export ${EXPORT}
    done

    if [[ $DEBUG ]]; then
        for EXPORT in $EXPORT_LIST; do
            echo "Exporting ${EXPORT}=${(P)EXPORT}"
        done
        echo $ATH_JOB_NAME
        echo $ATH_JOB_TIME
        echo $ATH_JOB_LOG
        echo $ATH_BACKEND_ARCH
        echo $ATH_BACKEND_NODES
        echo $ATH_BACKEND_EXE
        echo $ATH_TEST_ARCH
        echo $ATH_TEST_NODES
        echo $ATH_TEST_EXE
        echo $ATH_TEST_PROCS

        echo $ATH_JOB_NAME
        echo $ATH_JOB_NODE_COUNT
        echo $ATH_JOB_NODE_LIST
        echo $NODE_LIST
        echo $ATH_JOB_TIME
        echo $EXPORT_LIST
    fi


    sbatch --export=${(j:,:)EXPORT_LIST} \
        --output="${OUT_DIR}/${ATH_JOB_LOG}" \
        --job-name=${ATH_JOB_NAME} \
        --nodes=${ATH_JOB_NODE_COUNT} \
        --nodelist=${ATH_JOB_NODE_LIST} \
        --time=${ATH_JOB_TIME} \
        ${AURORA_LAUNCH_DIR}/mpi_launch.batch
}
