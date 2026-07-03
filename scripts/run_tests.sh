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
local ATH_JOB_NODE_LIST=( ${(s:,:)ATH_JOB_NODES} )
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
    if [[ $DEF_LOCAL_LAUNCH ]]; then
        ${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
            $JOB_NAME \
            'rome006' \
            'romebf3a006' \
            "${@[5,-1]}"
    else
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
    fi
}

# submit_test 'test script name.zsh' 'hh:mm:ss' 'number of nodes' \
#     'test name' \
#     10 \ # trial iterations 
#     '256' \ # extra parameters -> heatdis mem
#     '16' \ # extra parameters -> heatdis mpi procs
#     '8' # extra parameters -> headis server threads

submit_test 'heat_distribution_aurora.zsh' '08:00:00' '2' \
    'heatdis_aurora' \
    10 \
    '64' \
    '64' \
    '16'

# submit_test 'heat_distribution_aurora.zsh' '08:00:00' '2' \
#     'heatdis_aurora' \
#     10 \
#     '128' \
#     '64' \
#     '16'
#
# submit_test 'heat_distribution_aurora.zsh' '8:00:00' '2' \
#     'heatdis_aurora' \
#     10 \
#     '256' \
#     '64' \
#     '16'

submit_test 'heat_distribution_aurora.zsh' '10:00:00' '2' \
    'heatdis_aurora' \
    10 \
    '64' \
    '128' \
    '16'


submit_test 'heat_distribution_veloc.zsh' '08:00:00' '2' \
    'heatdis_veloc' \
    10 \
    '64' \
    '64' 

# submit_test 'heat_distribution_veloc.zsh' '08:00:00' '2' \
#     'heatdis_veloc' \
#     10 \
#     '128' \
#     '64' 
#
# submit_test 'heat_distribution_veloc.zsh' '08:00:00' '2' \
#     'heatdis_veloc' \
#     10 \
#     '256' \
#     '64' 

submit_test 'heat_distribution_veloc.zsh' '10:00:00' '2' \
    'heatdis_veloc' \
    10 \
    '64' \
    '128'
