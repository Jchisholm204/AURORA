#!/usr/bin/env zsh

# Test Launcher: Host

# Generic Front/Back-End SLURM Batch Launch Script
# -> Launches the test program with MPI
# -> Launches the backend on non duplicate hosts (1 backend per host)
# -> Hosts can be specified multiple times

function ath_launch_test(){
    local ATH_JOB_NAME=$1
    local ATH_JOB_LOG_DIR=$2 
    local ATH_TEST_EXE=$3
    local ATH_TEST_NODES=$4
    local ATH_TEST_PROCS=$5
    local ATH_BACKEND_EXE=$6
    local ATH_BACKEND_NODES=$7

    # Grab this here to ensure cluster exports/cc 
    #  does not break it.
    local USER=$(whoami)

    # Backend Launch
    source $AURORA_CLUSTER_DIR/env.sh 'x86_64'

    # len(split(csv)->array)->int
    local ATH_BACKEND_NODES_ARR=(${(s:,:)ATH_BACKEND_NODES})
    local N_BACKEND_NODES=${#ATH_BACKEND_NODES_ARR}

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
    source $AURORA_CLUSTER_DIR/env.sh "x86_64"

    mpirun \
        -np ${ATH_TEST_PROCS} \
        --oversubscribe \
        -H "${ATH_TEST_NODES}" \
        -x OPAL_PREFIX=${OPAL_PREFIX} \
        ${=ATH_TEST_EXE} \
        > "${ATH_JOB_LOG_DIR}/${ATH_JOB_NAME}_test.log" 2>&1


    # Wait for test to complete

    # Shutdown the server
    if kill -0 $BACKEND_PID 2>/dev/null; then
        kill -TERM $BACKEND_PID
        wait $BACKEND_PID 2>/dev/null
    fi

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

ath_launch_test \
    ${ATH_JOB_NAME} \
    ${ATH_JOB_LOG_DIR} \
    ${ATH_TEST_EXE} \
    ${ATH_TEST_NODES} \
    ${ATH_TEST_PROCS} \
    ${ATH_BACKEND_EXE} \
    ${ATH_BACKEND_NODES} \




