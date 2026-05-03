#!/usr/bin/env zsh

# BF Launch Script

function launch_bf() {
    local EXECUTABLE=$1

    # Check this is being called from a host device
    local L_ARCH=$(uname -m)
    if [[ $L_ARCH != "x86_64" ]]; then
        echo "This script must be launched from an x86_64 host"
        return 1
    fi

    if [[ ! -f "$EXECUTABLE" ]]; then
        echo "Executable $EXECUTABLE not found"
        return 2
    fi

    local PWD=$(pwd)

    local BF_HOSTNAME=$(hostname)

    echo "Launching $EXECUTABLE on $BF_HOSTNAME from $PWD with args ${@:3}"
    # ssh -n "$BF_HOSTNAME" "cd ${(q)PWD} && ${(q)EXECUTABLE} ${(@q)@:3}" &
    # local BF_SSH_PID=$!
    echo "Launched on PID: $BF_SSH_PID"
    return 0
}
