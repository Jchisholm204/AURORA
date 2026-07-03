#!/usr/bin/env zsh

# Heat Distribution AURORA Test

# All scripts re-source the ENV
source ${AURORA_SCRIPT_DIR}/env.sh

local JOB_NAME=$1
local LOG_DIR="${AURORA_LOG_DIR}/${JOB_NAME}"
local TEST_NODES=$2
local BACKEND_NODES=$3
local N_RUNS=$4
local MEM_MB=$5
local PROCS=$6
local BACKEND_PROCS=$7

echo "JOB: ${JOB_NAME}"
echo "LOG_DIR: ${LOG_DIR}"
echo "TEST_NODES: ${TEST_NODES}"
echo "BACKEND_NODES: ${BACKEND_NODES}"
echo "N_RUNS: ${N_RUNS}"
echo "MEM_MB: ${MEM_MB}"
echo "PROCS: ${PROCS}"
echo "BACKEND_PROCS: ${BACKEND_PROCS}"

local CHECKPOINT_DIR="${AURORA_CLUSTER_CHECKPOINT_DIR}/${JOB_NAME}_${MEM_MB}_${PROCS}_${N_RUNS}"
local TMP_DIR="${AURORA_CLUSTER_TMP_DIR}/${JOB_NAME}_${MEM_MB}_${PROCS}_${N_RUNS}"
local BUILD_DIR="${AURORA_CLUSTER_TMP_DIR}/${JOB_NAME}/build"
local TEST_BUILD_DIR="${BUILD_DIR}_test"
local BACKEND_BUILD_DIR="${BUILD_DIR}_backend"

function build_test_heat_distribution(){
    echo "${AURORA_CLUSTER_TMP_DIR}"
    mkdir -p "${BUILD_DIR}_test"
    mkdir -p "${BUILD_DIR}_backend"
    ${AURORA_LAUNCH_DIR}/build.zsh \
        ${TEST_BUILD_DIR} \
        ${BACKEND_BUILD_DIR} \
        ${BACKEND_PROCS}
}

function setup_test_heat_distribution(){
    mkdir -p $CHECKPOINT_DIR
    mkdir -p $TMP_DIR/checkpoints
}

function cleanup_test_heat_distribution(){
    rm -rf $CHECKPOINT_DIR
    rm -rf $TMP_DIR/checkpoints
}

function run_test_heat_distribution(){
    local ITERATION=$1
    setup_test_heat_distribution

    echo "Starting Heat Distribution Test:"
    echo "ITERATION=$ITERATION"
    ath_launch_test \
        "${JOB_NAME}_p${PROCS}_i${ITERATION}_m${MEM_MB}_b${BACKEND_PROCS}" \
        "${LOG_DIR}" \
        "${TEST_BUILD_DIR}/tests/heatdis_aurora ${MEM_MB} ${CHECKPOINT_DIR}" \
        "${TEST_NODES}" \
        ${PROCS} \
        "${BACKEND_BUILD_DIR}/server/aurora_remote_engine" \
        "${BACKEND_NODES}" 

    echo "Test Completed... Starting Restore.."

    ath_launch_test \
        "${JOB_NAME}_p${PROCS}_i${ITERATION}_m${MEM_MB}_b${BACKEND_PROCS}_restore" \
        "${LOG_DIR}" \
        "${TEST_BUILD_DIR}/tests/heatdis_aurora ${MEM_MB} ${CHECKPOINT_DIR}" \
        "${TEST_NODES}" \
        ${PROCS} \
        "${BACKEND_BUILD_DIR}/server/aurora_remote_engine" \
        "${BACKEND_NODES}" 

    echo "Test Completed... Cleaning Environment.."
    cleanup_test_heat_distribution
}

echo 'Starting Heat Distribution Test:'
mkdir -p "${LOG_DIR}"
mkdir -p "${TMP_DIR}"
build_test_heat_distribution

# Iterate over test conditions
for ((i = 0; i < $N_RUNS; i++)); do
    run_test_heat_distribution "${i}"
done

echo "Heat Distribution Test Concluded"
