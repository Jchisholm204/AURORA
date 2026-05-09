#!/usr/bin/env zsh

# Convert String back into array
export ATH_NODES=(${(z)ATH_NODES_STR})

echo $ATH_NODES

# Platform Details (arrays in CSV format)
local ATH_BACKEND_NODES=${(j:,:)ATH_NODES#*,}
local ATH_TEST_NODES=${(j:,:)ATH_NODES%%,*}

# SLURM Job details
local ATH_JOB_NAME="test_job"
local ATH_JOB_NODE_COUNT="$((((${#${(s:,:)ATH_BACKEND_NODES}} + ${#${(s:,:)ATH_TEST_NODES}}))))"
# Convert array into CSV format
local ATH_JOB_NODE_LIST=$( IFS=',';  echo "${ATH_NODES[*]}" )
local ATH_JOB_TIME="00:02:00"

ath_launch_test \
    $ATH_JOB_NAME \
    $ATH_JOB_TIME \
    "test.log" \
    "aarch64" \
    $ATH_BACKEND_NODES \
    "${AURORA_TOP_DIR}/build_BF/server/aurora_remote_engine" \
    "x86_64" \
    $ATH_TEST_NODES \
    "${AURORA_TOP_DIR}/build/test_discovery"

