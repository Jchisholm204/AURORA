#!/usr/bin/env zsh

# Ensure this env is setup properly
source ${AURORA_SCRIPT_DIR}/env.sh

# SLURM Job details
local ATH_JOB_NAME="test_job"
local ATH_JOB_TIME="00:05:00"


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

