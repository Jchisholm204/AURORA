#!/usr/bin/env zsh

# Arch Independent Environment Setup Script
# 

local ARCH_OPTIONS=( "aarch64" "x86_64" "none" )

function setup_env() {
    local ARCH=$1

    # Architecture of the compiler's host
    local LOCAL_ARCH=$(uname -m)

    echo "Setting up $ARCH environment for $LOCAL_ARCH"

    # Restore/Load Local ENVs
    export MODULEPATH="/global/software/rocky-9.$LOCAL_ARCH/modfiles/langs:/global/software/rocky-9.$LOCAL_ARCH/modfiles/tools:/global/software/rocky-9.$LOCAL_ARCH/modfiles/apps:/etc/modulefiles:/usr/share/modulefiles"

    module purge

    module load cmake

    export MODULEPATH="/global/software/rocky-9.$ARCH/modfiles/langs:/global/software/rocky-9.$ARCH/modfiles/tools:/global/software/rocky-9.$ARCH/modfiles/apps:/etc/modulefiles:/usr/share/modulefiles"

    module load gcc/11
    module load hpcx/2.20

    if [[ $ARCH != $LOCAL_ARCH && $ARCH == "aarch64" ]]; then
        echo "Loading Cross Compilation Tools"
        module use ~/.modules/modfiles
        module load gcc-arm/15.2
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

