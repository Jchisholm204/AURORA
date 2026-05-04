#!/usr/bin/env zsh

# odp0k specific test configuration script

# Slurm Extra Options
export SBATCH_PARTITION=''

# Export Possible Launch Pair Configurations
# Each should be "node-hostname,bf-hostname"
export ATH_NODES=(
    "odp0k001,odp0khna001"
    "odp0k002,odp0khna002"
    "odp0k003,odp0khna003"
    "odp0k004,odp0khna004"
)

# export ATH_BACKEND_PLATFORM="none"
export ATH_BACKEND_PLATFORM="bf"
