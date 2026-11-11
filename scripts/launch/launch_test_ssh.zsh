#!/usr/bin/env zsh

# Test Launcher: SSH

# Generic Front/Back-End SSH Launch Script
# -> Launches one backend per backend node over SSH
# -> Launches the test program with MPI on the client nodes
#
# Assumes passwordless/userless SSH between the nodes and that the repository,
# build, log, and checkpoint directories resolve to the same paths everywhere.

function ath_launch_test_ssh(){
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

    local -a ATH_BACKEND_ARR=(${(s:,:)ATH_BACKEND_NODES})
    local -a ATH_TARGETS ATH_PIDS

    local ATH_REMOTE_PREFIX=""
    if [[ -n "$AURORA_REMOTE_ENV" ]]; then
        ATH_REMOTE_PREFIX="source '${AURORA_REMOTE_ENV}' && "
    fi

    function _ath_ssh_cleanup(){
        local i
        for ((i = 1; i <= ${#ATH_PIDS}; i++)); do
            if [[ -n "${ATH_PIDS[i]}" ]]; then
                ssh ${=AURORA_SSH_OPTS} "${ATH_TARGETS[i]}" \
                    "kill -TERM ${ATH_PIDS[i]}" > /dev/null 2>&1
            fi
        done
    }

    # Ensure remote servers are cleaned up on any exit path
    trap '_ath_ssh_cleanup' EXIT INT TERM

    local ATH_BACKEND_NODE
    for ATH_BACKEND_NODE in $ATH_BACKEND_ARR; do
        local ATH_SSH_TARGET="${ATH_BACKEND_NODE}"
        if [[ -n "$AURORA_SSH_USER" ]]; then
            ATH_SSH_TARGET="${AURORA_SSH_USER}@${ATH_BACKEND_NODE}"
        fi
        ATH_TARGETS+=("$ATH_SSH_TARGET")

        local ATH_REMOTE_CMD="cd '${AURORA_TOP_DIR}' && { ${ATH_REMOTE_PREFIX}exec nohup '${ATH_BACKEND_EXE}' > '${ATH_JOB_LOG_DIR}/${ATH_JOB_NAME}_backend.log' 2>&1 & echo \$!; }"
        ATH_PIDS+=("$(ssh ${=AURORA_SSH_OPTS} "$ATH_SSH_TARGET" "$ATH_REMOTE_CMD")")
    done

    # time to bind
    sleep 2

    # Test Launch
    mpirun \
        -np ${ATH_TEST_PROCS} \
        --oversubscribe \
        -H "${ATH_TEST_NODES}" \
        -x OPAL_PREFIX=${OPAL_PREFIX} \
        ${=ATH_TEST_EXE} \
        > "${ATH_JOB_LOG_DIR}/${ATH_JOB_NAME}_test.log" 2>&1

    # Wait for test to complete

    # Shutdown the servers
    _ath_ssh_cleanup
    trap - EXIT INT TERM

    # Wait for server shutdown
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

ath_launch_test_ssh \
    ${ATH_JOB_NAME} \
    ${ATH_JOB_LOG_DIR} \
    ${ATH_TEST_EXE} \
    ${ATH_TEST_NODES} \
    ${ATH_TEST_PROCS} \
    ${ATH_BACKEND_EXE} \
    ${ATH_BACKEND_NODES}
