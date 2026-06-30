#!/usr/bin/env zsh

# Test Launcher: BlueField

# Generic Front/Back-End SLURM Batch Launch Script
# -> Launches the test and backend programs with srun and MPI

function ath_launch_test_bf(){
    local ATH_JOB_NAME=$1
    local ATH_JOB_LOG_DIR=$2 
    local ATH_TEST_EXE=$3
    local ATH_TEST_NODES=$4
    local ATH_TEST_PROCS=$5
    local ATH_BACKEND_EXE=$6
    local ATH_BACKEND_NODES=$7

    # Grab this here to ensure cluster exports/cc 
    #  does not break it.
    local USER=$USER

    # Backend Launch
    source $AURORA_CLUSTER_DIR/env.sh 'AArch64'

    # len(split(csv)->array)->int
    local N_BACKEND_NODES=${(s:,:)ATH_BACKEND_NODES}
    local N_BACKEND_NODES=${#N_BACKEND_NODES}

    srun --label \
        --export=ALL \
        --nodelist="${ATH_BACKEND_NODES}" \
        --nodes="${N_BACKEND_NODES}" \
        --job-name="ARE_${ATH_JOB_NAME}" \
        ${=ATH_BACKEND_EXE} \
        > "${ATH_JOB_LOG_DIR}/${ATH_JOB_NAME}_backend.log" 2>&1 &
    BACKEND_PID=$!

    # time to bind
    sleep 2

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

    # Shutdown the server
    scancel \
        --name="ARE_${ATH_JOB_NAME}" \
        --signal=TERM \
        --user=$USER

        # Wait for scancel
        sleep 2
}

# RUN

ATH_JOB_NAME=$1
ATH_JOB_LOG_DIR=$2 
ATH_TEST_EXE=$3
ATH_TEST_NODES=$4
ATH_TEST_PROCS=$5
ATH_BACKEND_EXE=$6
ATH_BACKEND_NODES=$7

ath_launch_test_bf \
    ${ATH_JOB_NAME} \
    ${ATH_JOB_LOG_DIR} \
    ${ATH_TEST_EXE} \
    ${ATH_TEST_NODES} \
    ${ATH_TEST_PROCS} \
    ${ATH_BACKEND_EXE} \
    ${ATH_BACKEND_NODES} \



