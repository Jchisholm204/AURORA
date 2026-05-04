#!/usr/bin/env zsh

# Convert String back into array
export ATH_NODES=(${(z)ATH_NODES_STR})

# SLURM Job details
export ATH_JOB_NAME="test_job"
export ATH_JOB_NODE_COUNT="1"
# Convert array into CSV format
export ATH_JOB_NODE_LIST=$( IFS=',';  echo "${ATH_NODES[*]}" )
export ATH_JOB_TIME="0:20:0"

# Platform Details (arrays in CSV format)
export ATH_BACKEND_NODES=${(j:,:)ATH_NODES#*,}
export ATH_TEST_NODES=${(j:,:)ATH_NODES%%,*}
echo "${ATH_BACKEND_NODES}"

${AURORA_TESTS_DIR}/test.batch

