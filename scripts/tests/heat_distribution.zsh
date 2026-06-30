#!/usr/bin/env zsh

# Heat Distribution AURORA Test

# All scripts re-source the ENV
source ${AURORA_SCRIPT_DIR}/env.sh

local CHECKPOINT_DIR=${AURORA_CLUSTER_CHECKPOINT_DIR}/heatdis
local TMP_DIR=${AURORA_CLUSTER_TMP_DIR}/heatdis
local BUILD_DIR=${AURORA_CLUSTER_TMP_DIR}/heatdis/build
local TEST_BUILD_DIR="${BUILD_DIR}_test"
local BACKEND_BUILD_DIR="${BUILD_DIR}_backend"

function build_test_heat_distribution(){
    echo "${AURORA_CLUSTER_TMP_DIR}"
    mkdir -p "${BUILD_DIR}_test"
    mkdir -p "${BUILD_DIR}_backend"
    ${AURORA_LAUNCH_DIR}/build.zsh \
        ${TEST_BUILD_DIR} \
        ${BACKEND_BUILD_DIR} \
        '16'
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
    local PROCS=$1
    local ITERATION=$2
    setup_test_heat_distribution

    ath_launch_test \
        "heatdis_${PROCS}_${ITERATION}" \
        "${AURORA_LOG_DIR}/heatdis" \
        "${TEST_BUILD_DIR}/tests/heatdis_aurora 256" \
        'rome005' \
        ${PROCS} \
        "${BACKEND_BUILD_DIR}/server/aurora_remote_engine" \
        'romebf3a005' 

    cleanup_test_heat_distribution
}


echo 'Starting Heat Distribution Test:'
for EXPORT in ${AURORA_EXPORT_LIST}; do
    echo "    ${EXPORT}: ${(P)EXPORT}"
done

build_test_heat_distribution

mkdir -p "${AURORA_LOG_DIR}/heatdis"

# Iterate over test conditions
local N_RUNS=1
for ((i = 0; i < $N_RUNS; i++)); do
    run_test_heat_distribution '2' "${i}"
done

echo "Heat Distribution Test Concluded"
