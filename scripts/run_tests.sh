#!/usr/bin/env zsh

source ./env.sh

# Submit jobs for discovery test -> 10 trials
${AURORA_TESTS_DIR}/discovery.sh 10
