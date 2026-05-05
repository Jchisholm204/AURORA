#!/usr/bin/env zsh

# ROME specific test configuration script

# Slurm Extra Options
export SBATCH_PARTITION=rome
export SLURM_PARTITION=$SBATCH_PARTITION

# Export Possible Launch Pair Configurations
# Each should be "node-hostname,bf-hostname"
typeset -gx ATH_NODES=(
    # Uncomment to use BF2 nodes
    # "rome001,romebf2a001"
    # "rome002,romebf2a002"
    # "rome003,romebf2a003"
    # "rome004,romebf2a004"

    # Uncomment to use BF3 nodes
    "rome005,romebf3a005"
    "rome006,romebf3a006"
    "rome007,romebf3a007"
    "rome008,romebf3a008"
)

# export ATH_BACKEND_PLATFORM="none"
export ATH_BACKEND_PLATFORM="bf"
