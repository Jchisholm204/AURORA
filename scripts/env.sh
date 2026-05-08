#!/usr/bin/env zsh


# -- BEGIN User Environment Variables -- 
#  Begin with '/' to set global directory, 
#   otherwise it is assumed to be a local directory
#   offset from the installation directory


AURORA_CHECKPOINT_DIR="checkpoints"
AURORA_CLUSTER_NAME="rome"
AURORA_LOG_DIR="results/0.0.1-1"

# -- END User Environment Variables -- 

function ath_setup_basedir(){
    # Find the toplevel of the project
    if [[ $(git rev-parse --is-inside-work-tree) == true ]]; then
        export AURORA_TOPLEVEL=$(git rev-parse --show-toplevel)
    else
        # Compatibility layer for tarballs
        printf "WARNING: Git repository not detected." >& 2
        printf "Using: %s as the top level directory." $(pwd) >& 2
        export AURORA_TOPLEVEL=$(pwd)
    fi
}

function ath_locate_directory(){
    local DIR=$1
    if [[ $DIR[1] == "/" ]]; then
        local DIR=$DIR
    else
        local DIR="${AURORA_TOPLEVEL}/${DIR}"
    fi

    echo "$DIR"

    if [[ ! -d $DIR ]]; then
        return 1
    else
        return 0
    fi
}

function ath_setup_directories(){
    # All AURORA paths declared here
    # Scripts ALWAYS use these paths rather than relative paths
    export AURORA_SCRIPT_DIR="${AURORA_TOPLEVEL}/scripts"
    export AURORA_TESTS_DIR="${AURORA_SCRIPT_DIR}/tests"
    export AURORA_LAUNCH_DIR="${AURORA_SCRIPT_DIR}/launchers"
    export AURORA_CLUSTER_DIR="${AURORA_SCRIPT_DIR}/clusters/${AURORA_CLUSTER_NAME}"

    # Fix Checkpointing Directory (user ENV)
    if [[ $AURORA_CHECKPOINT_DIR[1] = "/" ]]; then
    else
        export AURORA_CHECKPOINT_DIR="$AURORA_TOPLEVEL/$AURORA_CHECKPOINT_DIR"
    fi

    export AURORA_CHECKPOINT_DIR=$( ath_locate_directory "$AURORA_CHECKPOINT_DIR" )

    export AURORA_LOG_DIR=$( ath_locate_directory "$AURORA_LOG_DIR" )
}

function ath_verify_directories(){
    # Check the checkpoint directory is valid
    if [[ ! -d $AURORA_CHECKPOINT_DIR ]]; then
        mkdir -p "${AURORA_CHECKPOINT_DIR}"
        if [[ $? -ne 0 ]]; then
            printf "Warning: Specified Checkpoint Directory Unavaliable.\n" >& 2
            printf "Tests may fail.\n" >&2
        fi
    fi

    # Check the logging directory is valid
    if [[ ! -d $AURORA_LOG_DIR ]]; then
        mkdir -p "${AURORA_LOG_DIR}"
        if [[ $? -ne 0 ]]; then
            printf "Warning: Specified Logging Directory Unavaliable.\n" >& 2
            printf "Tests may fail.\n" >&2
        fi
    fi
}

function ath_verify_cluster_config(){
    local CLUSTER_NAME=$1
    local CLUSTER_DIR="${AURORA_SCRIPT_DIR}/clusters/${CLUSTER_NAME}"
    if [[ ! -d $CLUSTER_DIR ]]; then
        printf "Error: Cluster Directory Not Found\n" >&2
        printf "Cluster Directory must contain cluster configuration scripts.\n" >&2
        return 1
    fi

    if [[ ! -f "$CLUSTER_DIR/env.sh" ]]; then
        printf "Error: Cluster ENV script not found\n" >&2
        return 1
    fi
    if [[ ! -f "$CLUSTER_DIR/launch_config.sh" ]]; then
        printf "Error: Cluster Launch Configuration script not found\n" >&2
        return 2
    fi
}

function ath_source_cluster_config(){
    local CLUSTER_NAME=$1
    local CLUSTER_DIR="${AURORA_SCRIPT_DIR}/clusters/${CLUSTER_NAME}"
    ath_verify_cluster_config "$CLUSTER_NAME"
    source $AURORA_CLUSTER_DIR/launch_config.sh
    # Must compress the array into a string to share across scripts
    export ATH_NODES_STR="${(j: :)ATH_NODES}"
}


function launch_test(){
    
}


ath_setup_basedir
ath_setup_directories
ath_verify_directories
if ath_verify_cluster_config $AURORA_CLUSTER_NAME; then
    ath_source_cluster_config $AURORA_CLUSTER_NAME
fi
