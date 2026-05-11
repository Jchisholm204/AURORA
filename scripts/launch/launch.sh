#!/usr/bin/env zsh

# Sourced Wrapper Script for submitting sbatch jobs ('launch.batch')


function ath_launch_test(){
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
        # General/Directory Exports
        "AURORA_CLUSTER_DIR"
    )


    if [[ $DEBUG ]]; then
        for EXPORT in $EXPORT_LIST; do
            echo "Exporting ${EXPORT}=${(P)EXPORT}"
            export ${EXPORT}
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
        ${AURORA_LAUNCH_DIR}/launch.batch

}
