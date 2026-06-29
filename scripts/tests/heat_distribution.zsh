#!/usr/bin/env zsh

# Heat Distribution AURORA Test

# All scripts re-source the ENV
source ${AURORA_SCRIPT_DIR}/env.sh

local CHECKPOINT_DIR=${AURORA_CLUSTER_CHECKPOINT_DIR}/heatdis
local TMP_DIR=${AURORA_CLUSTER_TMP_DIR}/heatdis
local BUILD_DIR=${AURORA_CLUSTER_TMP_DIR}/heatdis/build

function build_test_heat_distribution(){
    ${AURORA_LAUNCH_DIR}/build.sh \
        'x86_64' \
        'ON' \
        ${BUILD_DIR}_x86_64 \
        '16'
    ${AURORA_LAUNCH_DIR}/build.sh \
        'AArch64' \
        'OFF' \
        ${BUILD_DIR}_AArch64 \
        '16'
}

function setup_test_heat_distribution(){
    mkdir -p $CHECKPOINT_DIR
    mkdir -p $TMP_DIR
}

function cleanup_test_heat_distribution(){
    rm -rf $CHECKPOINT_DIR
    rm -rf $TMP_DIR
}

function run_test_heat_distribution(){
    local PROCS=$1
    setup_test_heat_distribution

    ath_launch_test_mpi \
        "build/tests/block_test ${mem_kb} ${CHECKPOINT_DIR}" \
            ${procs} ${CHECKPOINT_DIR}

    cleanup_test_heat_distribution
}


echo 'Starting Heat Distribution Test:'
for EXPORT in ${AURORA_EXPORT_LIST}; do
    echo "    ${EXPORT}: ${(P)EXPORT}"
done

# build_test_heat_distribution

# Iterate over test conditions
local N_RUNS=$1
for ((i = 0; i < $N_RUNS; i++)); do
    echo "$i"
done

rm -rf ${BUILD_DIR}*

echo "Heat Distribution Test Concluded"
