#!/usr/bin/env zsh

# BF Launch Script


export BF2_NODES=( \
    "romebf2a001" \
    "romebf2a002" \
    "romebf2a003" \
    "romebf2a004" \
)

export BF3_NODES=( \
    "romebf3a005" \
    "romebf3a006" \
    "romebf3a007" \
    "romebf3a008" \
)

function launch_bf() {
    local BF_HOSTNAME=$1
    local EXECUTABLE=$2

    if [[ $# < 2 ]]; then
        echo "Expected: arg1=bf_hostname arg2=exe"
        echo "Got $# arguments instead"
        return 1
    fi

    if [[ ${BF2_NODES[(r)$BF_HOSTNAME]} != $BF_HOSTNAME && ${BF3_NODES[(r)$BF_HOSTNAME]} != $BF_HOSTNAME ]]; then
        echo "Invalid BF Hostname. Expected:"
        echo "\t$BF2_NODES"
        echo "\t$BF3_NODES"
        echo "Got: hostname=$BF_HOSTNAME"
        return 1
    fi

    local L_ARCH=$(uname -m)
    if [[ $L_ARCH != "x86_64" ]]; then
        echo "This script must be launched from an x64 host"
        return 2
    fi

    if [[ ! -f "$EXECUTABLE" ]]; then
        echo "Executable $EXECUTABLE not found"
        return 2
    fi

    local PWD=$(pwd)

    echo "Launching $EXECUTABLE on $BF_HOSTNAME from $PWD with args ${@:3}"
    ssh -n "$BF_HOSTNAME" "cd ${(q)PWD} && ${(q)EXECUTABLE} ${(@q)@:3}" &
    export BF_SSH_PID=$!
    echo "Launched on PID: $BF_SSH_PID"
    return 0
}


