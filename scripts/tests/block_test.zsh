#!/usr/bin/env zsh

# 64 MB
local MEM_KB_MIN=$((64*1024))
# 256 GB
local MEM_KB_MAX=$((256*1024*1024))
# local MEM_KB_MAX=$((128*1024))

local PROCS_MIN=8
local PROCS_MAX=128
# local PROCS_MAX=16

function blocking_get_build_dir(){
    local ARCH=$1
    local WORKERS=$2
    if [[ $WORKERS ]]; then
        echo "${AURORA_TOP_DIR}/build_blocking_w${WORKERS}_${ARCH}"
    else
        echo "${AURORA_TOP_DIR}/build_${ARCH}"
    fi
}

function build_test_blocking() {
    local ARCH=$1
    local WORKERS=$2

    local BUILD_TESTS='OFF'

    if [[ $ARCH == "x86_64" ]]; then
        BUILD_TESTS='ON'
    fi

    echo "Building ${ARCH} w=${WORKERS}"

    ${AURORA_LAUNCH_DIR}/build.sh \
        "${ARCH}" \
        "${BUILD_TESTS}" \
        $(blocking_get_build_dir ${ARCH} ${WORKERS}) \
        ${WORKERS}
}

function run_test_blocking() {
    local ITERATIONS=$1
    local WORKERS=$2

    if [[ ! $ITERATIONS ]]; then
        local ITERATIONS=1
    fi

    local N_NODES=${#ATH_NODES}


    for ((i = 0; i < ITERATIONS; i++)); do
        for ((mem_kb = MEM_KB_MIN; mem_kb <= MEM_KB_MAX; mem_kb=mem_kb*2)); do
            for ((procs = PROCS_MIN; procs <= PROCS_MAX; procs=procs*2)); do
                local CHECKPOINT_DIR="$HOME/exafs/checkpoints/${i}_${mem_kb}_${procs}_${WORKERS}"
                mkdir -p $CHECKPOINT_DIR
                local TS_PAIR=${ATH_NODES[ $((i % N_NODES + 1)) ]}
                ath_launch_test_mpi \
                    "blocking" \
                    "00:40:00" \
                    "$(date +%Y%m%d_%H%M)_${i}_${mem_kb}kb_${procs}p_${WORKERS}w.log" \
                    "aarch64" \
                    ${TS_PAIR#*,} \
                    "$(blocking_get_build_dir 'aarch64' $WORKERS)/server/aurora_remote_engine" \
                    "x86_64" \
                    ${TS_PAIR%%,*} \
                    "$(blocking_get_build_dir 'x86_64' $WORKERS)/tests/block_test ${mem_kb} ${CHECKPOINT_DIR}" \
                    ${procs} ${CHECKPOINT_DIR}
                done
            done
    done
    
}

# -- Run Test Discovery

local ITERATIONS=$1

# Ensure this env is setup properly
source ${AURORA_SCRIPT_DIR}/env.sh
# Dont build if nobuild is specified
if [[ ! $NOBUILD ]]; then
    build_test_blocking 'aarch64' '8'
    build_test_blocking 'aarch64' '16'
    build_test_blocking 'aarch64' '32'
    build_test_blocking 'x86_64' '8'
    build_test_blocking 'x86_64' '16'
    build_test_blocking 'x86_64' '32'
fi
run_test_blocking $ITERATIONS '8'
run_test_blocking $ITERATIONS '16'
run_test_blocking $ITERATIONS '32'

# -- Run Test Discovery

