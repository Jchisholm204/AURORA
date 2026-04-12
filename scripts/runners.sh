#!/usr/bin/env zsh

# Runner Functions

function run_valgrind() {
    local EXECUTABLE=$1
    local LOG_FILE="${EXECUTABLE}"
    if [[ $2 ]]; then
        LOG_FILE=$2
    fi
	valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --verbose --error-limit=no \
             --log-file="${LOG_FILE}_valgrind.out" \
             $EXECUTABLE
}

function run_callgrind() {
    local EXECUTABLE=$1
    local LOG_FILE="${EXECUTABLE}"
    if [[ $2 ]]; then
        LOG_FILE=$2
    fi
	valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --verbose --error-limit=no \
             --log-file="${LOG_FILE}_valgrind.out" \
             --tool=callgrind \
             --dump-instr=yes \
             --callgrind-out-file="${LOG_FILE}_callgrind.out" \
             $EXECUTABLE
}

function run_cachegrind() {
    local EXECUTABLE=$1
    local LOG_FILE="${EXECUTABLE}"
    if [[ $2 ]]; then
        LOG_FILE=$2
    fi
	valgrind --tool=callgrind \
             --dump-instr=yes \
             --callgrind-out-file="${LOG_FILE}_callgrind_out" \
             $EXECUTABLE
}

function run_mpi() {
    local EXECUTABLE=$1
    local PROCS=$(( $(nproc) / 2))
    if [[ $2 ]]; then
        PROCS=$2
    fi
    echo "MPI: procs=$PROCS executable=$EXECUTABLE"
    mpirun -np $PROCS $EXECUTABLE 
}

