#!/usr/bin/env zsh

# Test Evironment Management Functions

export AURORA_TOPLEVEL=$(git rev-parse --show-toplevel)
export AURORA_SCRIPT_DIR="${AURORA_TOPLEVEL}/scripts"
export AURORA_TESTS_DIR="${AURORA_TOPLEVEL}/scripts/tests"
export AURORA_CHECKPOINT_DIR="${AURORA_TOPLEVEL}/checkpoints"

function init_test_env() {
    if [[ ! -d "$AURORA_CHECKPOINT_DIR" ]]; then
        mkdir "$AURORA_CHECKPOINT_DIR"
    fi
}

function cleanup_test_env() {
    if [[ -d "$AURORA_CHECKPOINT_DIR" ]]; then
        rm -rf "$AURORA_CHECKPOINT_DIR"
    fi
}
