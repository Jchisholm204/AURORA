#!/usr/bin/env zsh

# Heat Distribution Test (NO Checkpointing No Nothing)

# All scripts re-source the ENV
source ${AURORA_SCRIPT_DIR}/env.sh

local JOB_NAME=$1
local LOG_DIR="${AURORA_LOG_DIR}/${JOB_NAME}"
local TEST_NODES=$2
local N_RUNS=$4
local MEM_MB=$5
local PROCS=$6

echo "JOB: ${JOB_NAME}"
echo "LOG_DIR: ${LOG_DIR}"
echo "TEST_NODES: ${TEST_NODES}"
echo "N_RUNS: ${N_RUNS}"
echo "MEM_MB: ${MEM_MB}"
echo "PROCS: ${PROCS}"

local TMP_DIR=${AURORA_CLUSTER_TMP_DIR}/${JOB_NAME}_${MEM_MB}_${PROCS}_${N_RUNS}
local BUILD_DIR=${TMP_DIR}/build

function build_test_heat_distribution(){
    echo "${AURORA_CLUSTER_TMP_DIR}"
    mkdir -p "${BUILD_DIR}"
    ${AURORA_LAUNCH_DIR}/build.zsh \
        ${BUILD_DIR} \
        ${BUILD_DIR} \
        ${BACKEND_PROCS}
}

function setup_test_heat_distribution(){
}

function cleanup_test_heat_distribution(){
}

function run_test_heat_distribution(){
    local ITERATION=$1
    setup_test_heat_distribution

    echo "Starting Heat Distribution Test:"
    echo "ITERATION=$ITERATION"
    ${AURORA_SCRIPT_DIR}/launch/launch_test_none.zsh \
        "${JOB_NAME}_p${PROCS}_i${ITERATION}_m${MEM_MB}" \
        "${LOG_DIR}" \
        "${BUILD_DIR}/tests/heatdis_original ${MEM_MB} useless_arg" \
        "${TEST_NODES}" \
        ${PROCS} \

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

