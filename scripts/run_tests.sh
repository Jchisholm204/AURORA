#!/usr/bin/env zsh

source ${AURORA_SCRIPT_DIR}/env.sh

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
    # Launch without sbatch
    # ${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
    #     $JOB_NAME \
    #     'rome005' \
    #     'romebf3a005' \
    #     "${@[5,-1]}"
        # $JOB_TEST_NODES \
        # $JOB_BACKEND_NODES \
    # Launch with Sbatch
    sbatch --export=${(j:,:)AURORA_EXPORT_LIST} \
        --output="${AURORA_LOG_DIR}/${JOB_NAME}/slurm-%j.log" \
        --job-name=${JOB_NAME} \
        --nodes='2' \
        --nodelist='rome006,romebf3a006' \
        --time="${JOB_TIME}" \
        ${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
            $JOB_NAME \
            'rome006' \
            'romebf3a006' \
            "${@[5,-1]}"
        # --nodelist='rome005,romebf3a005' \
}

# submit_test 'heat_distribution_aurora.zsh' '02:00:00' '2' \
#     'heatdis_aurora8' \
#     10 \
#     '256' \
#     '16' \
#     '8'
# submit_test 'heat_distribution_aurora.zsh' '02:00:00' '2' \
#     'heatdis_aurora16' \
#     10 \
#     '256' \
#     '16' \
#     '16'

# submit_test 'heat_distribution_aurora.zsh' '02:00:00' '2' \
#     'heatdis_aurora8' \
#     10 \
#     '256' \
#     '32' \
#     '8'
# submit_test 'heat_distribution_aurora.zsh' '02:00:00' '2' \
#     'heatdis_aurora16' \
#     10 \
#     '256' \
#     '32' \
#     '16'

# submit_test 'heat_distribution_aurora.zsh' '02:00:00' '2' \
#     'heatdis_aurora16' \
#     10 \
#     '256' \
#     '64' \
#     '16'
#
# submit_test 'heat_distribution_aurora.zsh' '02:00:00' '2' \
#     'heatdis_aurora16' \
#     10 \
#     '256' \
#     '128' \
#     '16'


submit_test 'heat_distribution_veloc.zsh' '02:00:00' '2' \
    'heatdis_veloc' \
    10 \
    '256' \
    '32' \

# submit_test 'heat_distribution_aurora.zsh' '02:00:00' '2' \
#     'heatdis_aurora16' \
#     10 \
#     '256' \
#     '128' \
#     '16'
