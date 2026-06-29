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
    if [[ "${AURORA_BACKEND_PLATFORM}" == 'bf' ]]; then
    ${AURORA_LAUNCH_DIR}/build.sh \
        'AArch64' \
        'OFF' \
        ${BUILD_DIR}_AArch64 \
        '16'
    fi
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
    local ITERATION=$2
    setup_test_heat_distribution

    ath_launch_test \
        "heatdis_${PROCS}_${ITERATION}" \
        "${AURORA_LOG_DIR}/heatdis" \
        'text_exe' \
        'test_nodes' \
        ${PROCS} \
        'backend_exe' \
        'backend_nodes' 
        

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
