#!/usr/bin/env zsh


function run_test_discovery() {
    local ITERATIONS=$1
    if [[ ! $ITERATIONS ]]; then
        local ITERATIONS=1
    fi

    local N_NODES=${#ATH_NODES}

    for ((i = 0; i < $ITERATIONS; i++)); do
        local TS_PAIR=${ATH_NODES[ $((i % N_NODES + 1)) ]}
        ath_launch_test \
            "discovery" \
            "00:05:00" \
            "$(date +%Y%m%d_%H%M)_${i}.log" \
            "aarch64" \
            ${TS_PAIR#*,} \
            "${AURORA_TOP_DIR}/build_aarch64/server/aurora_remote_engine" \
            "x86_64" \
            ${TS_PAIR%%,*} \
            "${AURORA_TOP_DIR}/build_x86_64/tests/test_discovery"
    done
    
}

# -- Run Test Discovery

# Ensure this env is setup properly
source ${AURORA_SCRIPT_DIR}/env.sh
run_test_discovery $1

# -- Run Test Discovery
