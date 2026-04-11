#!/usr/bin/env zsh

function setup_env() {
    local ARCH=$1
    export MODULEPATH="/global/software/rocky-9.$ARCH/modfiles/langs:/global/software/rocky-9.$ARCH/modfiles/tools:/global/software/rocky-9.$ARCH/modfiles/apps:/etc/modulefiles:/usr/share/modulefiles"

    echo $ARCH
}

ARCH=$(uname -m)
setup_env $ARCH
