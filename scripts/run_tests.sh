#!/usr/bin/env zsh

source ${AURORA_TOP_DIR}/env.sh

if [[ $DEBUG ]]; then
    echo ${ATH_NODES}
    echo ${AURORA_LOG_DIR}
    echo "EXPORTS:"
    for EXPORT in ${AURORA_EXPORT_LIST}; do
        echo "    ${EXPORT}: ${(P)EXPORT}"
    done
fi

local ATH_JOB_NODES="${ATH_TEST_NODES},${ATH_BACKEND_NODES}"
local ATH_JOB_NODE_LIST=( ${(s:,:)ATH_JOB_NODE_LIST} )
export ATH_JOB_NODE_COUNT=${#ATH_JOB_NODE_LIST}
export AURORA_EXPORT_LIST

function submit_test(){
    local JOB_SCRIPT=$1
    local JOB_TIME=$2
    local JOB_N_NODES=$3
    local JOB_NAME=$4
    echo "Running JOB:"
    echo " NAME: ${JOB_NAME}"
    echo " TIME: ${JOB_TIME}"
    echo " SCRIPT: ${JOB_SCRIPT}"
    ${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
        $JOB_NAME \
        'rome005' \
        'romebf3a005' \
        "${@[5,-1]}"
        # $JOB_TEST_NODES \
        # $JOB_BACKEND_NODES \
    # sbatch --export=${(j:,:)EXPORT_LIST} \
    #     --output="${AURORA_LOG_DIR}/${JOB_NAME}/slurm.log" \
    #     --job-name=${JOB_NAME} \
    #     --nodes=${ATH_JOB_NODE_COUNT} \
    #     --nodelist=${ATH_JOB_NODES} \
    #     --time="${JOB_TIME}" \
    #     ${AURORA_TESTS_DIR}/${JOB_SCRIPT}
}

# submit_test 'discovery' '00:45:00' 'heat_distribution.zsh' '10'

submit_test 'heat_distribution.zsh' '00:45:00' '2' \
    'heatdis_aurora' \
    1 \
    '256' \
    '16'

