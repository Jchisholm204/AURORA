#!/usr/bin/env zsh

# Test Launcher

# Generic Front/Back-End Launch Script
# -> Mode host: Launches the backend and test on the local node
# -> Mode slurm: Launches the backend with srun and the test with MPI
# -> Mode none: Launches only the test

function ath_launch_test(){
    local ATH_JOB_NAME=$1
    local ATH_JOB_LOG_DIR=$2 
    local ATH_TEST_EXE=$3
    local ATH_TEST_NODES=$4
    local ATH_TEST_PROCS=$5
    local ATH_BACKEND_EXE=$6
    local ATH_BACKEND_NODES=$7

    local ATH_LAUNCHER="${AURORA_LAUNCH_DIR}/launch_test_${AURORA_LAUNCH_PLATFORM}.zsh"
    if [[ ! -f "$ATH_LAUNCHER" ]]; then
        printf "Error: Unknown launch platform '%s'\n" "$AURORA_LAUNCH_PLATFORM" >&2
        return 1
    fi
    
    "$ATH_LAUNCHER" \
        ${ATH_JOB_NAME} \
        ${ATH_JOB_LOG_DIR} \
        ${ATH_TEST_EXE} \
        ${ATH_TEST_NODES} \
        ${ATH_TEST_PROCS} \
        ${ATH_BACKEND_EXE} \
        ${ATH_BACKEND_NODES} \
        > "${ATH_JOB_LOG_DIR}/${ATH_JOB_NAME}.log" 2>&1
}

# RUN

# ATH_JOB_NAME=$1
# ATH_JOB_LOG_DIR=$2 
# ATH_TEST_EXE=$3
# ATH_TEST_NODES=$4
# ATH_TEST_PROCS=$5
# ATH_BACKEND_EXE=$6
# ATH_BACKEND_NODES=$7
#
# ath_launch_test \
#     ${ATH_JOB_NAME} \
#     ${ATH_JOB_LOG_DIR} \
#     ${ATH_TEST_EXE} \
#     ${ATH_TEST_NODES} \
#     ${ATH_TEST_PROCS} \
#     ${ATH_BACKEND_EXE} \
#     ${ATH_BACKEND_NODES} 
