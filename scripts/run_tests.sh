#!/usr/bin/env zsh

# Convert String back into array
export ATH_NODES=(${(z)ATH_NODES_STR})

# Platform Details (arrays in CSV format)
local ATH_BACKEND_NODES=${(j:,:)ATH_NODES#*,}
local ATH_TEST_NODES=${(j:,:)ATH_NODES%%,*}

# SLURM Job details
local ATH_JOB_NAME="test_job"
# Convert array into CSV format
local ATH_JOB_TIME="00:05:00"

# Source the launch script
source ${AURORA_LAUNCH_DIR}/launch.sh

ath_launch_test \
    $ATH_JOB_NAME \
    $ATH_JOB_TIME \
    "test.log" \
    "aarch64" \
    $ATH_BACKEND_NODES \
    "${AURORA_TOP_DIR}/build_aarch64/server/aurora_remote_engine" \
    "x86_64" \
    $ATH_TEST_NODES \
    "${AURORA_TOP_DIR}/build_x86_64/tests/test_discovery"

