#!/usr/bin/env zsh

function test_discovery(){
    # -- BEGIN: Check Environment Variables --

    if [ -n ${ATH_NODES_STR} ]; then
        echo "Error: Source 'scripts/env.sh' before continuing." >&2
        exit 1
    fi

    # Convert String back into array
    export ATH_NODES=(${(z)ATH_NODES_STR})

    # -- END: Check Environment Variables --

    # -- BEGIN: Setup Test Variables --

    export ATH_JOB_NAME="discovery"
    export ATH_JOB_NODES=$( IFS=',';  echo "${ATH_NODES[*]}" )
    export ATH_JOB_TIME="00:02:00"
    export ATH_JOB_LOG="${AURORA_LOG_DIR}/${ATH_JOB_NAME}/x.log"
    # -- END: Setup Test Variables --
}

# -- BEGIN: Check Environment Variables --

if [ -n ${ATH_NODES_STR} ]; then
    echo "Error: Source 'scripts/env.sh' before continuing." >&2
    exit 1
fi

# Convert String back into array
export ATH_NODES=(${(z)ATH_NODES_STR})

# -- END: Check Environment Variables --

# -- BEGIN: Setup Test Variables --

export ATH_JOB_NAME="discovery"
export ATH_JOB_NODES=$( IFS=',';  echo "${ATH_NODES[*]}" )
export ATH_JOB_TIME="00:02:00"
export ATH_JOB_LOG="${AURORA_LOG_DIR}/${ATH_JOB_NAME}/x.log"

# Platform Details (arrays in CSV format)
export ATH_BACKEND_NODES=${(j:,:)ATH_NODES#*,}
export ATH_TEST_NODES=${(j:,:)ATH_NODES%%,*}

# SLURM Job details
export ATH_JOB_NAME="test_job"
export ATH_JOB_NODE_COUNT="$((((${#${(s:,:)ATH_BACKEND_NODES}} + ${#${(s:,:)ATH_TEST_NODES}}))))"
# Convert array into CSV format

echo "$ATH_JOB_NODE_COUNT"
sbatch --export=ALL \
    --output="idk.log" \
    --job-name=${ATH_JOB_NAME} \
    --nodes=${ATH_JOB_NODE_COUNT} \
    --nodelist=${ATH_JOB_NODE_LIST} \
    --time=${ATH_JOB_TIME} \
    ${AURORA_TESTS_DIR}/test.batch

