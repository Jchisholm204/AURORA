#!/usr/bin/env zsh

# Made for odp0k
# Assumes GCC module has been installed on path

source /usr/share/Modules/init/zsh
module purge
module use ~/.modules/modfiles
module use /global/modules/modfiles
module purge
module load mpi/openmpi-x86_64
module load veloc
