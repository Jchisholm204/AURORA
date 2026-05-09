#!/usr/bin/env zsh

# Sourced Wrapper Script for submitting sbatch jobs ('launch.batch')


function ath_launch_test(){
    # Helper function to launch a slurm test
    local ATH_JOB_NAME=$1
    local ATH_JOB_TIME=$2
    local ATH_JOB_LOG=$3
    local ATH_BACKEND_ARCH=$4
    local ATH_BACKEND_NODES=$5
    local ATH_BACKEND_EXE=$6
    local ATH_TEST_ARCH=$7
    local ATH_TEST_NODES=$8
    local ATH_TEST_EXE=$9


    sbatch --export=ALL \
        --output="idk.log" \
        --job-name=${ATH_JOB_NAME} \
        --nodes=${ATH_JOB_NODE_COUNT} \
        --nodelist=${ATH_JOB_NODE_LIST} \
        --time=${ATH_JOB_TIME} \
        ${AURORA_TESTS_DIR}/test.batch

}
