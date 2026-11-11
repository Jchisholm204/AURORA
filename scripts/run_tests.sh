#!/usr/bin/env zsh

source "${0:A:h}/env.sh"

# -- Test Selection --
# Toggle each test family on/off with true/false.
#
# AURORA heat distribution sweep: AURORA checkpoint/restart at several memory
# sizes and MPI process counts (64 and 128 procs).
RUN_HEATDIS_AURORA=false
#
# VeloC heat distribution sweep: VeloC checkpoint/restart benchmark run through
# the client-only launcher at the same memory sizes (64 and 128 procs).
RUN_HEATDIS_VELOC=false
#
# Original heat distribution benchmark: the unmodified heatdis program, used as
# a baseline against the AURORA and VeloC variants (64 procs).
RUN_HEATDIS_ORIGINAL=false
#
# AURORA blocking read/write heatmap: block_test_aurora at 16 GB per process
# with 128 MPI procs, sweeping the backend worker count (8, 16, 32).
RUN_BLOCK_AURORA=true
#
# AURORA blocking read/write heatmap with DPU-offloaded restore: same test as
# above, compiled with AUL_DEFAULT_RESTART_REMOTE so restores run on the DPU.
RUN_BLOCK_AURORA_OFFLOAD=false

# -- Test Parameters --
# Number of trial iterations to run for every test
TEST_N_RUNS=2
# Heat distribution sweep memory range in MB, doubled each step
HEATDIS_MEM_MIN=64
HEATDIS_MEM_MAX=1024
# Block test heatmap memory (MB) and MPI process count
HEATMAP_MEM='16'
HEATMAP_PROCS='4'

if [[ $DEBUG ]]; then
	echo ${AURORA_CLUSTER_NODES_STR}
	echo ${AURORA_LOG_DIR}
	echo "EXPORTS:"
	for EXPORT in ${AURORA_EXPORT_LIST}; do
		echo "    ${EXPORT}: ${(P)EXPORT}"
	done
fi

export AURORA_EXPORT_LIST

function submit_test() {
	local JOB_SCRIPT=$1
	local JOB_TIME=$2
	local JOB_NAME=$3
	local N_NODES=$4
	shift 4
	local -a TEST_ARGS=("$@")

	# Use the first N_NODES configured pairs, clamped to what is available
	local N_AVAILABLE=${#AURORA_CLUSTER_NODES}
	local N_NODES=${N_NODES:-1}
	if [[ $N_NODES -lt 1 ]]; then
		local N_NODES=1
	fi
	if [[ $N_NODES -gt $N_AVAILABLE ]]; then
		local N_NODES=$N_AVAILABLE
	fi

	local -a NODE_PAIRS=(${AURORA_CLUSTER_NODES[1,$N_NODES]})
	local -a TEST_NODE_ARR BACKEND_NODE_ARR
	local NODE_ENTRY
	for NODE_ENTRY in $NODE_PAIRS; do
		local -a NODE_PAIR=(${(s:,:)NODE_ENTRY})
		TEST_NODE_ARR+=("${NODE_PAIR[1]}")
		BACKEND_NODE_ARR+=("${NODE_PAIR[2]:-${NODE_PAIR[1]}}")
	done
	local TEST_NODES="${(j:,:)TEST_NODE_ARR}"
	local BACKEND_NODES="${(j:,:)BACKEND_NODE_ARR}"

	# Job names are not suffixed with node names; multi-node jobs share one
	# name because they are launched as one aggregated job.
	if [[ $N_NODES -eq 1 ]]; then
		local JOB_FULL_NAME="${JOB_NAME}"
	else
		local JOB_FULL_NAME="${JOB_NAME}_multi"
	fi
	local LOG_DIR="${AURORA_LOG_DIR}/${JOB_FULL_NAME}"
	mkdir -p "$LOG_DIR"

	echo "Running JOB:"
	echo " NAME: ${JOB_FULL_NAME}"
	echo " TIME: ${JOB_TIME}"
	echo " SCRIPT: ${JOB_SCRIPT}"
	echo " N_NODES: ${N_NODES}"
	echo " TEST_NODES: ${TEST_NODES}"
	echo " BACKEND_NODES: ${BACKEND_NODES}"

	if [[ $AURORA_LAUNCH_PLATFORM == (slurm|ssh) ]]; then
		# One aggregated job: multiple client nodes, one server per backend
		if [[ $AURORA_LAUNCH_PLATFORM == "slurm" ]]; then
			# Allocate the unique union of client and backend nodes
			local -a USED_NODES=(${(u)TEST_NODE_ARR})
			USED_NODES+=(${(u)BACKEND_NODE_ARR})
			USED_NODES=(${(u)USED_NODES})
			local ALLOC_NODES=${#USED_NODES}
			local NODELIST="${(j:,:)USED_NODES}"

			sbatch --export=${(j:,:)AURORA_EXPORT_LIST} \
				--output="${LOG_DIR}/slurm-%j.log" \
				--job-name="${JOB_FULL_NAME}" \
				--nodes="${ALLOC_NODES}" \
				--nodelist="${NODELIST}" \
				--partition="${SBATCH_PARTITION}" \
				--time="${JOB_TIME}" \
				${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
				"${JOB_FULL_NAME}" \
				"${TEST_NODES}" \
				"${BACKEND_NODES}" \
				"${TEST_ARGS[@]}"
		else
			# SSH: server launch is handled by the launcher
			${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
				"${JOB_FULL_NAME}" \
				"${TEST_NODES}" \
				"${BACKEND_NODES}" \
				"${TEST_ARGS[@]}"
		fi
	else
		# host/none: each pair is an independent local run
		local PAIR
		for PAIR in $NODE_PAIRS; do
			local -a NODE_PAIR=(${(s:,:)PAIR})
			local TEST_NODE=${NODE_PAIR[1]}
			local BACKEND_NODE=${NODE_PAIR[2]:-${NODE_PAIR[1]}}
			local PAIR_JOB_NAME="${JOB_NAME}"
			echo "Running JOB:"
			echo " NAME: ${PAIR_JOB_NAME}"
			echo " TEST_NODE: ${TEST_NODE}"
			echo " BACKEND_NODE: ${BACKEND_NODE}"
			${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
				"${PAIR_JOB_NAME}" \
				"${TEST_NODE}" \
				"${BACKEND_NODE}" \
				"${TEST_ARGS[@]}"
		done
	fi
}

# submit_test 'test script name.zsh' 'hh:mm:ss' \
#     'test name' \
#     1 \ # N_NODES (node pairs used by this job)
#     10 \ # trial iterations
#     '256' \ # extra parameters -> heatdis mem
#     '16' \ # extra parameters -> heatdis mpi procs
#     '8' # extra parameters -> headis server threads

for ((i = $HEATDIS_MEM_MIN; i <= $HEATDIS_MEM_MAX; i = i * 2)); do
	echo "$i"
	# Run AURORA Tests
	if [[ $RUN_HEATDIS_AURORA == "true" ]]; then
		submit_test 'heat_distribution_aurora.zsh' '08:00:00' \
			'heatdis_aurora' \
			'1' \
			"${TEST_N_RUNS}" \
			"$i" \
			'64' \
			'16'

		submit_test 'heat_distribution_aurora.zsh' '08:00:00' \
			'heatdis_aurora' \
			'1' \
			"${TEST_N_RUNS}" \
			"$i" \
			'128' \
			'16'
	fi

	# Run VeloC Tests
	if [[ $RUN_HEATDIS_VELOC == "true" ]]; then
		submit_test 'heat_distribution_veloc.zsh' '08:00:00' \
			'heatdis_veloc' \
			'1' \
			"${TEST_N_RUNS}" \
			"$i" \
			'64'

		submit_test 'heat_distribution_veloc.zsh' '08:00:00' \
			'heatdis_veloc' \
			'1' \
			"${TEST_N_RUNS}" \
			"$i" \
			'128'
	fi

	# Run Original (unmodified) heatdis benchmark
	if [[ $RUN_HEATDIS_ORIGINAL == "true" ]]; then
		submit_test 'heat_distribution.zsh' '08:00:00' \
			'heatdis_orig' \
			'1' \
			"${TEST_N_RUNS}" \
			"$i" \
			'64'

		submit_test 'heat_distribution.zsh' '08:00:00' \
			'heatdis_orig' \
			'1' \
			"${TEST_N_RUNS}" \
			"$i" \
			'64'
	fi
done

# Heatmap Tests
if [[ $RUN_BLOCK_AURORA == "true" ]]; then
	submit_test 'block_test_aurora.zsh' '4:00:00' \
		'block_test_aurora_heatmap' \
		'1' \
		"${TEST_N_RUNS}" \
		"${HEATMAP_MEM}" \
		"${HEATMAP_PROCS}" \
		'8'

	# submit_test 'block_test_aurora.zsh' '4:00:00' \
	# 	'block_test_aurora_heatmap' \
	# 	'1' \
	# 	"${TEST_N_RUNS}" \
	# 	"${HEATMAP_MEM}" \
	# 	"${HEATMAP_PROCS}" \
	# 	'16'
	#
	# submit_test 'block_test_aurora.zsh' '4:00:00' \
	# 	'block_test_aurora_heatmap' \
	# 	'1' \
	# 	"${TEST_N_RUNS}" \
	# 	"${HEATMAP_MEM}" \
	# 	"${HEATMAP_PROCS}" \
	# 	'32'
fi

# DPU-offloaded restart heatmap tests
if [[ $RUN_BLOCK_AURORA_OFFLOAD == "true" ]]; then
	submit_test 'block_test_aurora_offload.zsh' '4:00:00' \
		'block_test_aurora_offload_heatmap' \
		'1' \
		"${TEST_N_RUNS}" \
		"${HEATMAP_MEM}" \
		"${HEATMAP_PROCS}" \
		'8'
fi
