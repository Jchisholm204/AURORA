#!/usr/bin/env zsh

# odp0k specific test configuration script

# Slurm Extra Options
export SBATCH_PARTITION=''
export SLURM_PARTITION=$SBATCH_PARTITION

# Export Possible Launch Pair Configurations
# Each should be "node-hostname,bf-hostname"
# export AURORA_CLUSTER_NODES=(
#     "odp0k001,odp0khna001"
#     "odp0k002,odp0khna002"
#     "odp0k003,odp0khna003"
#     "odp0k004,odp0khna004"
# )

export AURORA_CLUSTER_NODES=(
    "odp0k001"
    "odp0k002"
    "odp0k003"
    "odp0k004"
)

# Set the backend platform (bf (for arm devices), host, none)
export AURORA_BACKEND_PLATFORM="none"
# export AURORA_CLUSTER_BACKEND_PLATFORM="bf"

# Set the checkpoint directory to use on this cluster
export AURORA_CLUSTER_CHECKPOINT_DIR="/tmp/barf_persistent"

# Temporary directory to use during testing
export AURORA_CLUSTER_TMP_DIR='/tmp/barf'
