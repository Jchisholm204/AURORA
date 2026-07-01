#!/usr/bin/env zsh

# Made for odp0k
# Assumes GCC module has been installed on path

source /usr/share/Modules/init/zsh
module use ~/.modules/modfiles
module purge
module load mpi/openmpi-x86_64
module load veloc

