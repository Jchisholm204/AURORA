#!/usr/bin/env zsh


export ATH_JOB_NAME="test_job"
export ATH_JOB_NODE_COUNT="1"
echo ${ATH_NODES[@]}
export ATH_JOB_NODE_LIST=${ATH_NODES[@]}
export ATH_JOB_TIME="0:20:0"

${AURORA_TESTS_DIR}/test.batch

