#!/usr/bin/env zsh


# -- BEGIN User Environment Variables -- 
#  Begin with '/' to set global directory, 
#   otherwise it is assumed to be a local directory
#   offset from the installation directory


local AURORA_CHECKPOINT_DIR="checkpoints"
local AURORA_CLUSTER_NAME="rome"
local AURORA_LOG_DIR="results/0.0.1-1"

# -- END User Environment Variables -- 

function _ath_finddir_basedir(){
    # Helper function to find the base repository directory (pure)
    # Returns by echo, return value indicates git (0) or fallback (1)
    if [[ $(git rev-parse --is-inside-work-tree 2>& /dev/null) == true ]]; then
        # Return through stdo (fd=1)
        printf "%s" $(git rev-parse --show-toplevel)
        return 0
    else
        # Compatibility layer for tarballs
        printf "WARNING: Git repository not detected.\n" >& 2
        printf "Using: %s as the top level directory.\n" $(pwd) >& 2
        # Return through stdo (fd=1)
        printf "%s" $(pwd)
        return 1
    fi
}

function _ath_finddir_abspath(){
    # Converts a relative (within this repo) or absolute path
    # to an absolute path. (pure)
    # Returns by echo, value indicates success (0) or failure (1)
    local DIR=$1
    if [[ $DIR[1] == "/" ]]; then
        local DIR=$DIR
    else
        local DIR="$(_ath_finddir_basedir 2>& /dev/null)/${DIR}"
    fi

    echo "$DIR"

    if [[ ! -d $DIR ]]; then
        printf "Error: Directory '%s' not found.\n" $DIR >& 2
        return 1
    else
        return 0
    fi
}

function ath_setup_env(){
    # All AURORA paths declared here (impure)
    # Scripts ALWAYS use these paths rather than relative paths
    AURORA_TOP_DIR=$(_ath_finddir_basedir)
    if [[ $? -ne 0 ]]; then
        printf "Fallback: Using %s as the top level directory.\n" $AURORA_TOP_DIR >& 2
    fi

    AURORA_SCRIPT_DIR="${AURORA_TOP_DIR}/scripts"
    AURORA_TESTS_DIR="${AURORA_SCRIPT_DIR}/tests"
    AURORA_LAUNCH_DIR="${AURORA_SCRIPT_DIR}/launch"
    AURORA_CLUSTER_DIR="${AURORA_SCRIPT_DIR}/clusters/${AURORA_CLUSTER_NAME}"
    AURORA_CHECKPOINT_DIR=$(_ath_finddir_abspath "$AURORA_CHECKPOINT_DIR")
    mkdir -p $AURORA_CHECKPOINT_DIR
    if [[ $? -ne 0 ]]; then
        printf "Warning: Invalid Checkpoint Directory.\n" >& 2
    fi
    AURORA_LOG_DIR=$(_ath_finddir_abspath "$AURORA_LOG_DIR")
    mkdir -p $AURORA_LOG_DIR
    if [[ $? -ne 0 ]]; then
        printf "Warning: Invalid Logging Directory.\n" >& 2
    fi
    export AURORA_TOP_DIR
    export AURORA_SCRIPT_DIR
    export AURORA_TESTS_DIR 
    export AURORA_LAUNCH_DIR 
    export AURORA_CLUSTER_DIR 
    export AURORA_CHECKPOINT_DIR
    export AURORA_LOG_DIR
    source ${AURORA_LAUNCH_DIR}/launch.sh
}

function ath_verify_cluster_config(){
    # Verifies that the cluster configuration contains the correct scripts
    # (impure) $1 - Cluster Name
    local CLUSTER_NAME=$1
    local CLUSTER_DIR="${AURORA_SCRIPT_DIR}/clusters/${CLUSTER_NAME}"
    if [[ ! -d $CLUSTER_DIR ]]; then
        printf "Error: Cluster Directory Not Found\n" >&2
        printf "Cluster Directory must contain cluster configuration scripts.\n" >&2
        return 1
    fi

    if [[ ! -f "$CLUSTER_DIR/env.sh" ]]; then
        printf "Error: Cluster ENV script not found\n" >&2
        return 2
    fi
    if [[ ! -f "$CLUSTER_DIR/launch_config.sh" ]]; then
        printf "Error: Cluster Launch Configuration script not found\n" >&2
        return 3
    fi
    return 0
}

function ath_source_cluster_config(){
    local CLUSTER_NAME=$1
    local CLUSTER_DIR="${AURORA_SCRIPT_DIR}/clusters/${CLUSTER_NAME}"
    ath_verify_cluster_config "$CLUSTER_NAME"
    # Source Launch Configuration
    source $AURORA_CLUSTER_DIR/launch_config.sh
    # Must compress the array into a string to share across scripts
    export ATH_NODES_STR="${(j: :)ATH_NODES}"
    # Seperate the lists for easy access in tests
    # -> Platform Details (arrays in CSV format)
    export ATH_BACKEND_NODES=${(j:,:)ATH_NODES#*,}
    export ATH_TEST_NODES=${(j:,:)ATH_NODES%%,*}
}

ath_setup_env
if ath_verify_cluster_config $AURORA_CLUSTER_NAME; then
    ath_source_cluster_config $AURORA_CLUSTER_NAME
fi
