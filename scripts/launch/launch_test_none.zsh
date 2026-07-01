#!/usr/bin/env zsh

# Test Launcher: None

# Launches only the test program. Useful if testing with persistent servers.
# Backend launch must be performed externally for tests to work.

function ath_launch_test_none(){
    local ATH_JOB_NAME=$1
    local ATH_JOB_LOG_DIR=$2 
    local ATH_TEST_EXE=$3
    local ATH_TEST_NODES=$4
    local ATH_TEST_PROCS=$5

    # Grab this here to ensure cluster exports/cc 
    #  does not break it.
    local USER=$(whoami)

    # Test Launch
    source $AURORA_CLUSTER_DIR/env.sh 'x86_64'

    mpirun \
        -np ${ATH_TEST_PROCS} \
        --oversubscribe \
        -H "${ATH_TEST_NODES}" \
        -x OPAL_PREFIX=${OPAL_PREFIX} \
        ${=ATH_TEST_EXE} \
        > "${ATH_JOB_LOG_DIR}/${ATH_JOB_NAME}_test.log" 2>&1
    # Wait for test to complete
}

# RUN

ATH_JOB_NAME=$1
ATH_JOB_LOG_DIR=$2 
ATH_TEST_EXE=$3
ATH_TEST_NODES=$4
ATH_TEST_PROCS=$5

ath_launch_test_none \
    ${ATH_JOB_NAME} \
    ${ATH_JOB_LOG_DIR} \
    ${ATH_TEST_EXE} \
    ${ATH_TEST_NODES} \
    ${ATH_TEST_PROCS} \




