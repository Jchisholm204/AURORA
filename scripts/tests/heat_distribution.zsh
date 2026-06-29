#!/usr/bin/env zsh

# Heat Distribution AURORA Test


function setup_test_heat_distribution(){

}

function cleanup_test_heat_distribution(){

}

function run_test_heat_distribution(){

}


echo "Starting Heat Distribution Test"
echo "EXPORTS:"
for EXPORT in ${AURORA_EXPORT_LIST}; do
    echo "    ${EXPORT}: ${(P)EXPORT}"
done

# Iterate over test conditions
local N_RUNS=$1
for ((i = 0; i < $N_RUNS; i++)); do
    echo "$i"
done

echo "Heat Distribution Test Concluded"
