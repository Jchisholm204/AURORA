#!/usr/bin/env zsh


# -- User Environment Variables -- 
#  Begin with '/' to set global directory, 
#   otherwise it is assumed to be a local directory
#   offset from the installation directory


AURORA_CHECKPOINT_DIR="checkpoints"
AURORA_CLUSTER_DIR="odp0k"

# -- User Environment Variables -- 

# -- Directory Setup -- 

export AURORA_TOPLEVEL=$(git rev-parse --show-toplevel)
export AURORA_SCRIPT_DIR="${AURORA_TOPLEVEL}/scripts"
export AURORA_TESTS_DIR="${AURORA_TOPLEVEL}/scripts/tests"
# Resource user variables properly
if [[ $AURORA_CHECKPOINT_DIR[1] = "/" ]]; then
    export AURORA_CHECKPOINT_DIR=$AURORA_CHECKPOINT_DIR
else
    export AURORA_CHECKPOINT_DIR="$AURORA_TOPLEVEL/$AURORA_CHECKPOINT_DIR"
fi

# Check the checkpoint directory is valid
if [[ ! -d $AURORA_CHECKPOINT_DIR ]]; then
    mkdir $AURORA_CHECKPOINT_DIR
    if [[ $? -ne 0 ]]; then
        echo "Warning: Specified Checkpoint Directory Unavaliable. Tests may fail." >&2
    fi
fi

if [[ ! -d $AURORA_CLUSTER_DIR ]]; then
    echo "Error: Cluster Directory Not Found" >&2
    return 1
else
    export AURORA_TESTS_DIR="${AURORA_TOPLEVEL}/scripts/$AURORA_CLUSTER_DIR"
fi

# -- Directory Setup -- 

# -- Source Scripts -- 
source $AURORA_SCRIPT_DIR/launch_bf.sh
source $AURORA_SCRIPT_DIR/runners.sh

if [[ ! -f "$AURORA_CLUSTER_DIR/env.sh" ]]; then
    echo "Error: Cluster ENV script not found" >&2
    return 1
else
    source $AURORA_CLUSTER_DIR/env.sh
fi
