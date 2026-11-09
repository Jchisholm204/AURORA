#!/usr/bin/env zsh

source "${0:A:h}/env.sh"

if [[ $DEBUG ]]; then
    echo ${AURORA_CLUSTER_NODES_STR}
    echo ${AURORA_LOG_DIR}
    echo "EXPORTS:"
    for EXPORT in ${AURORA_EXPORT_LIST}; do
        echo "    ${EXPORT}: ${(P)EXPORT}"
    done
fi

export AURORA_EXPORT_LIST

function submit_test(){
    local JOB_SCRIPT=$1
    local JOB_TIME=$2
    local JOB_N_NODES=$3
    local JOB_NAME=$4

    local NODE_ENTRY
    for NODE_ENTRY in $AURORA_CLUSTER_NODES; do
        local -a NODE_PAIR=(${(s:,:)NODE_ENTRY})
        local TEST_NODE=${NODE_PAIR[1]}
        local BACKEND_NODE=${NODE_PAIR[2]:-${NODE_PAIR[1]}}
        local JOB_FULL_NAME="${JOB_NAME}_${TEST_NODE}"
        local LOG_DIR="${AURORA_LOG_DIR}/${JOB_FULL_NAME}"
        mkdir -p "$LOG_DIR"

        echo "Running JOB:"
        echo " NAME: ${JOB_FULL_NAME}"
        echo " TIME: ${JOB_TIME}"
        echo " SCRIPT: ${JOB_SCRIPT}"
        echo " TEST_NODE: ${TEST_NODE}"
        echo " BACKEND_NODE: ${BACKEND_NODE}"

        if [[ $AURORA_LAUNCH_PLATFORM == "slurm" ]]; then
            local N_NODES=${#NODE_PAIR}
            local NODELIST="${TEST_NODE}"
            if [[ $N_NODES -ge 2 ]]; then
                NODELIST="${TEST_NODE},${BACKEND_NODE}"
            fi
            # Launch with Sbatch
            sbatch --export=${(j:,:)AURORA_EXPORT_LIST} \
                --output="${LOG_DIR}/slurm-%j.log" \
                --job-name="${JOB_FULL_NAME}" \
                --nodes="${N_NODES}" \
                --nodelist="${NODELIST}" \
                --partition="${SBATCH_PARTITION}" \
                --time="${JOB_TIME}" \
                ${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
                "${JOB_FULL_NAME}" \
                "${TEST_NODE}" \
                "${BACKEND_NODE}" \
                "${@[5,-1]}"
        else
            # Launch without sbatch
            ${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
                "${JOB_FULL_NAME}" \
                "${TEST_NODE}" \
                "${BACKEND_NODE}" \
                "${@[5,-1]}"
        fi
    done
}

# submit_test 'test script name.zsh' 'hh:mm:ss' 'number of nodes' \
#     'test name' \
#     10 \ # trial iterations 
#     '256' \ # extra parameters -> heatdis mem
#     '16' \ # extra parameters -> heatdis mpi procs
#     '8' # extra parameters -> headis server threads

HEATDIS_MEM_MIN=64
HEATDIS_MEM_MAX=1024
for ((i = $HEATDIS_MEM_MIN; i <= $HEATDIS_MEM_MAX; i=i*2)); do
    echo "$i"
    # Run AURORA Tests
    submit_test 'heat_distribution_aurora.zsh' '08:00:00' '2' \
        'heatdis_aurora' \
        10 \
        "$i" \
        '64' \
        '16'

    submit_test 'heat_distribution_aurora.zsh' '08:00:00' '2' \
        'heatdis_aurora' \
        10 \
        "$i" \
        '128' \
        '16'

    # Run VeloC Tests
    submit_test 'heat_distribution_veloc.zsh' '08:00:00' '2' \
        'heatdis_veloc' \
        10 \
        "$i" \
        '64'

    submit_test 'heat_distribution_veloc.zsh' '08:00:00' '2' \
        'heatdis_veloc' \
        10 \
        "$i" \
        '128'

    # Run Original (unmodified) heatdis benchmark
    submit_test 'heat_distribution.zsh' '08:00:00' '2' \
        'heatdis_orig' \
        10 \
        "$i" \
        '64' 

    submit_test 'heat_distribution.zsh' '08:00:00' '2' \
        'heatdis_orig' \
        10 \
        "$i" \
        '64' 
done


# Heatmap Tests
# Mem in MB
HEATMAP_MEM='16384'
HEATMAP_PROCS='128'
submit_test 'block_test_aurora.zsh' '4:00:00' '2' \
    'block_test_aurora_heatmap' \
    10 \
    "${HEATMAP_MEM}" \
    "${HEATMAP_PROCS}" \
    '8'

submit_test 'block_test_aurora.zsh' '4:00:00' '2' \
    'block_test_aurora_heatmap' \
    10 \
    "${HEATMAP_MEM}" \
    "${HEATMAP_PROCS}" \
    '16'

submit_test 'block_test_aurora.zsh' '4:00:00' '2' \
    'block_test_aurora_heatmap' \
    10 \
    "${HEATMAP_MEM}" \
    "${HEATMAP_PROCS}" \
    '32'
