#!/usr/bin/env zsh

# Arch Independent Environment Setup Script
# Made for the Dummy cluster
# Assumes a lot of undocumented things...

local ARCH_OPTIONS=( "aarch64" "x86_64" "none" )

# Setup local or cross compilation env
function setup_env() {
    local ARCH=$1

    # Architecture of the compiler's host
    local LOCAL_ARCH=$(uname -m)

    echo "Setting up $ARCH environment for $LOCAL_ARCH"

    if [[ $ARCH != $LOCAL_ARCH && $ARCH == "aarch64" ]]; then
        echo "Loading Cross Compilation Tools"
        module use $HOME/.modules/modfiles
        module use /global/modules/modfiles
        module load hpcx/2.51-aarch64
        module load gcc-arm/15.2
    fi

    if [[ "${ARCH}" == 'aarch64' ]]; then
        echo "aarch64 environment ready"
        module use /global/modules/modfiles
        module load hpcx/2.51-aarch64
    fi

    if [[ "${ARCH}" == 'x86_64' ]]; then
        module use $HOME/.modules/modfiles
        module use /global/modules/modfiles
        module load veloc
        module load hpcx/2.51-x86_64
    fi
    return 0
}

# Default Option - Use Host Arch
local ARCH=$(uname -m)

if [[ $1 ]]; then
    # Check 
    if [[ ${ARCH_OPTIONS[(r)$1]} == $1 ]]; then
        ARCH=$1
    else
        echo "Invalid Arch Option. "
        echo "Valid Options = {$ARCH_OPTIONS}"
        return -1
    fi
fi

if [[ $ARCH == "none" ]]; then
    return 0
else
    setup_env $ARCH
fi

