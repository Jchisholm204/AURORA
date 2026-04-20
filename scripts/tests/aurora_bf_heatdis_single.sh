#!/usr/bin/env zsh
#SBATCH --nodes=2
#SBATCH --partition=rome
#SBATCH --nodelist=rome005,romebf3a005
#SBATCH --time=00:5:00

cd $HOME/scratch/aurora-project
