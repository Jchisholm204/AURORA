#!/usr/bin/env zsh

# ROME specific test configuration script

# Slurm Extra Options
export SBATCH_PARTITION=''
export SLURM_PARTITION=$SBATCH_PARTITION

# Export Possible Launch Pair Configurations
# Each should be "node-hostname,bf-hostname"
export AURORA_CLUSTER_NODES=(
	# "dummy01,dummy01bf1"
	"dummy02,dummy02bf1"
)

# Set the launch platform (host, slurm, none)
# slurm with a "client,backend" pair launches the backend on the BlueField
export AURORA_LAUNCH_PLATFORM="ssh"

# Set the checkpoint directory to use on this cluster
export AURORA_CLUSTER_CHECKPOINT_DIR="/mnt/beegfs/checkpoints"

# Temporary directory to use during testing
export AURORA_CLUSTER_TMP_DIR="$HOME/mnt/beegfs/tmp"
