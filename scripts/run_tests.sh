#!/usr/bin/env zsh

# Convert String back into array
export ATH_NODES=(${(z)ATH_NODES_STR})

echo $ATH_NODES

# Platform Details (arrays in CSV format)
export ATH_BACKEND_NODES=${(j:,:)ATH_NODES#*,}
export ATH_TEST_NODES=${(j:,:)ATH_NODES%%,*}

# SLURM Job details
export ATH_JOB_NAME="test_job"
export ATH_JOB_NODE_COUNT="$((((${#${(s:,:)ATH_BACKEND_NODES}} + ${#${(s:,:)ATH_TEST_NODES}}))))"
# Convert array into CSV format
export ATH_JOB_NODE_LIST=$( IFS=',';  echo "${ATH_NODES[*]}" )
export ATH_JOB_TIME="00:02:00"

echo "$ATH_JOB_NODE_COUNT"
sbatch --export \
    --output="idk.log" \
    --job-name=${ATH_JOB_NAME} \
    --nodes=${ATH_JOB_NODE_COUNT} \
    --nodelist=${ATH_JOB_NODE_LIST} \
    --time=${ATH_JOB_TIME} \
    ${AURORA_TESTS_DIR}/test.batch

