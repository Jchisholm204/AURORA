#!/usr/bin/env zsh

# 64 MB
local MEM_KB_MIN=$((64*1024))
# 256 GB
# local MEM_KB_MAX=((256*1024*1024))
local MEM_KB_MAX=$((64*1024))

local PROCS_MIN=8
# local PROCS_MAX=128
local PROCS_MAX=16

function run_test_blocking() {
    local ITERATIONS=$1
    if [[ ! $ITERATIONS ]]; then
        local ITERATIONS=1
    fi

    local N_NODES=${#ATH_NODES}

    for ((i = 0; i < ITERATIONS; i++)); do
        for ((mem_kb = MEM_KB_MIN; mem_kb <= MEM_KB_MAX; mem_kb=mem_kb*2)); do
            for ((procs = PROCS_MIN; procs <= PROCS_MAX; procs=procs*2)); do
                local TS_PAIR=${ATH_NODES[ $((i % N_NODES + 1)) ]}
                ath_launch_test_mpi \
                    "blocking" \
                    "00:20:00" \
                    "$(date +%Y%m%d_%H%M)_${i}_${mem_kb}kb_${procs}p.log" \
                    "aarch64" \
                    ${TS_PAIR#*,} \
                    "${AURORA_TOP_DIR}/build_aarch64/server/aurora_remote_engine" \
                    "x86_64" \
                    ${TS_PAIR%%,*} \
                    "${AURORA_TOP_DIR}/build_x86_64/tests/block_test ${mem_kb}" \
                    ${procs}
                done
            done
    done
    
}

# -- Run Test Discovery

# Ensure this env is setup properly
source ${AURORA_SCRIPT_DIR}/env.sh
run_test_blocking $1

# -- Run Test Discovery

