#!/usr/bin/env zsh

# Arch Independent Environment Setup Script
# 

local ARCH_OPTIONS=( "aarch64" "x86_64" "none" )

function setup_env() {
    local ARCH=$1

    echo "Setting up $ARCH environment"

    module purge

    module load cmake
    # module use ~/.modules/modfiles
    # module load gcc-arm

    export MODULEPATH="/global/software/rocky-9.$ARCH/modfiles/langs:/global/software/rocky-9.$ARCH/modfiles/tools:/global/software/rocky-9.$ARCH/modfiles/apps:/etc/modulefiles:/usr/share/modulefiles"

    module load gcc/11
    module load hpcx/2.20
    
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

