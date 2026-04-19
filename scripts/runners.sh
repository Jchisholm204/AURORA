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

function run_mpi_log() {
    local EXECUTABLE=$1
    local MEM_KB=$2
    
    # Default PROCS to half of nproc if $3 is not provided
    local PROCS=$(( $(nproc) / 2 ))
    if [[ -n $3 ]]; then
        PROCS=$3
    fi

    # 1. Define a dynamic log filename
    # Example: run_myprog_mem1024kb_p4.log
    local LOG_FILE="run_${EXECUTABLE##*/}_mem${MEM_KB}kb_p${PROCS}.log"

    # 2. Status message
    echo "------------------------------------------------"
    echo "MPI Config:  $EXECUTABLE"
    echo "Processors:  $PROCS"
    echo "Memory (KB): $MEM_KB"
    echo "Logging to:  $LOG_FILE"
    echo "------------------------------------------------"

    # 3. Execute and redirect
    # '2>&1' captures errors, 'tee' writes to file and screen
    mpirun -np $PROCS $EXECUTABLE $MEM_KB 2>&1 | tee "$LOG_FILE"
}

function run_mpi_batch() {
    local EXECUTABLE=$1
    local MEM_KB=$2
    local ITERATIONS=5
    
    # Default PROCS to half of nproc if $3 is not provided
    local PROCS=$(( $(nproc) / 2 ))
    if [[ -n $3 ]]; then
        PROCS=$3
    fi

    echo "Preparing to run $ITERATIONS iterations..."

    for (( i=1; i<=$ITERATIONS; i++ ))
    do
        # 1. Define log filename with a run counter (e.g., run1, run2...)
        local LOG_FILE="run_${EXECUTABLE##*/}_mem${MEM_KB}kb_p${PROCS}_run${i}.log"

        echo "--- [Run $i/$ITERATIONS] Logging to: $LOG_FILE ---"

        # 2. Execute with the iteration number visible in the console
        mpirun -np $PROCS $EXECUTABLE $MEM_KB 2>&1 | tee "$LOG_FILE"
        
        echo "--- [Run $i] Finished ---"
        echo ""
    done
}
