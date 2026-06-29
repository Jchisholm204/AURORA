#!/usr/bin/env zsh

# Test Launcher

# Generic Front/Back-End SLURM Batch Launch Script
# -> Launches the test program with srun and MPI

function ath_launch_test(){
    local ATH_JOB_NAME=$1
    local ATH_JOB_LOG=$2 
    local ATH_TEST_EXE=$3
    local ATH_TEST_NODES=$4
    local ATH_BACKEND_EXE=$5
    local ATH_BACKEND_NODES=$6
}

echo "LAUNCH: mpi_launch.batch"
echo "JOB_NAME: ${ATH_JOB_NAME}"
echo "JOB_TIMEOUT: ${ATH_JOB_TIME}"
echo "JOB_LOG: ${ATH_JOB_LOG}"
echo "PARTITION?: ${SBATCH_PARTITION}"
echo "BACKEND_ARCH: ${ATH_BACKEND_ARCH}"
echo "BACKEND_NODES: ${ATH_BACKEND_NODES}"
echo "BACKEND_EXE: ${ATH_BACKEND_EXE}"
echo "TEST_ARCH: ${ATH_TEST_ARCH}"
echo "TEST_NODES: ${ATH_TEST_NODES}"
echo "TEST_EXE: ${ATH_TEST_EXE}"
echo "MPI_PROCS: ${ATH_TEST_PROCS}"

# Grab this here to ensure cluster exports/cc 
#  does not break it.
local USER=$USER

# Reformat arrays from CSV lists
local ATH_BACKEND_NODES=( ${(s:,:)ATH_BACKEND_NODES} )
local ATH_TEST_NODES=( ${(s:,:)ATH_TEST_NODES} )

# Backend Launch
source $AURORA_CLUSTER_DIR/env.sh "$ATH_BACKEND_ARCH"

srun --label \
    --export=ALL \
    --nodelist="${(j:,:)ATH_BACKEND_NODES}" \
    --nodes="${#ATH_BACKEND_NODES}" \
    --job-name="ARE_${ATH_JOB_NAME}" \
    ${=ATH_BACKEND_EXE} &
BACKEND_PID=$!

# time to bind
sleep 2

# Test Launch
source $AURORA_CLUSTER_DIR/env.sh "$ATH_TEST_ARCH"

mpirun \
    -np ${ATH_TEST_PROCS} \
    --oversubscribe \
    -H "${(j:,:)ATH_TEST_NODES}" \
    -x OPAL_PREFIX=${OPAL_PREFIX} \
    ${=ATH_TEST_EXE}


# Wait for test to complete

# Shutdown the server
scancel --name="ARE_${ATH_JOB_NAME}" \
    --signal=TERM \
    --user=$USER

# Wait for scancel
sleep 2

echo "[$(date)] BENCHMARK RUN FINISHED"
