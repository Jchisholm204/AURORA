#!/usr/bin/env zsh

# Heat Distribution VeloC Test

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
local CHECKPOINT_DIR="${AURORA_CLUSTER_CHECKPOINT_DIR}/${JOB_NAME}_${MEM_MB}_${PROCS}_${N_RUNS}_checkpoints"
local TMP_CHECKPOINT_DIR="${AURORA_CLUSTER_CHECKPOINT_DIR}/${JOB_NAME}_${MEM_MB}_${PROCS}_${N_RUNS}_tmp_checkpoints"
local TMP_META_DIR="/tmp/${JOB_NAME}_${MEM_MB}_${PROCS}_${N_RUNS}_meta"
local BUILD_DIR=${TMP_DIR}/build

function build_test_heat_distribution(){
    echo "${AURORA_CLUSTER_TMP_DIR}"
    mkdir -p "${BUILD_DIR}"
    ${AURORA_LAUNCH_DIR}/build.zsh \
        ${BUILD_DIR} \
        ${BUILD_DIR} \
        ${BACKEND_PROCS}

    # Create the config file
    cat << EOF > "${TMP_DIR}/veloc_heatdis.cfg"
scratch = ${TMP_CHECKPOINT_DIR}
persistent = ${CHECKPOINT_DIR}
meta = ${TMP_META_DIR}
max_versions = 2
scratch_versions = 2
mode = async
chksum = false

EOF
}

function setup_test_heat_distribution(){
    mkdir -p $CHECKPOINT_DIR
    rm -r $CHECKPOINT_DIR
    mkdir -p $CHECKPOINT_DIR
    mkdir -p $TMP_CHECKPOINT_DIR
    rm -r $TMP_CHECKPOINT_DIR
    mkdir -p $TMP_CHECKPOINT_DIR
    mkdir -p $TMP_META_DIR
    rm -r $TMP_META_DIR
    mkdir -p $TMP_META_DIR

}

function cleanup_test_heat_distribution(){
    rm -rf $CHECKPOINT_DIR
    rm -rf $TMP_CHECKPOINT_DIR
    rm -rf $TMP_META_DIR
    pkill veloc-backend
}

function run_test_heat_distribution(){
    local ITERATION=$1
    setup_test_heat_distribution

    echo "Starting Heat Distribution Test:"
    echo "ITERATION=$ITERATION"
    ${AURORA_SCRIPT_DIR}/launch/launch_test_none.zsh \
        "${JOB_NAME}_p${PROCS}_i${ITERATION}_m${MEM_MB}" \
        "${LOG_DIR}" \
        "${BUILD_DIR}/tests/heatdis_veloc_mem ${MEM_MB} ${TMP_DIR}/veloc_heatdis.cfg" \
        "${TEST_NODES}" \
        ${PROCS} \

    echo "Test Completed... Starting Restore.."

    ${AURORA_SCRIPT_DIR}/launch/launch_test_none.zsh \
        "${JOB_NAME}_p${PROCS}_i${ITERATION}_m${MEM_MB}_restore" \
        "${LOG_DIR}" \
        "${BUILD_DIR}/tests/heatdis_veloc_mem ${MEM_MB} ${TMP_DIR}/veloc_heatdis.cfg" \
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

