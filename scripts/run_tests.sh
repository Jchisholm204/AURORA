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

	local NODE_ENTRY
	for NODE_ENTRY in $AURORA_CLUSTER_NODES; do
		local -a NODE_PAIR=(${(s:,:)NODE_ENTRY})
		local TEST_NODE=${NODE_PAIR[1]}
		local BACKEND_NODE=${NODE_PAIR[2]:-${NODE_PAIR[1]}}
		local JOB_FULL_NAME="${JOB_NAME}_${TEST_NODE}"
		local LOG_DIR="${AURORA_LOG_DIR}/${JOB_FULL_NAME}"
		mkdir -p "$LOG_DIR"

		echo "Running JOB:"
		echo " NAME: ${JOB_FULL_NAME}"
		echo " TIME: ${JOB_TIME}"
		echo " SCRIPT: ${JOB_SCRIPT}"
		echo " TEST_NODE: ${TEST_NODE}"
		echo " BACKEND_NODE: ${BACKEND_NODE}"

		if [[ $AURORA_LAUNCH_PLATFORM == "slurm" ]]; then
			local N_NODES=${#NODE_PAIR}
			local NODELIST="${TEST_NODE}"
			if [[ $N_NODES -ge 2 ]]; then
				NODELIST="${TEST_NODE},${BACKEND_NODE}"
			fi
			# Launch with Sbatch
			sbatch --export=${(j:,:)AURORA_EXPORT_LIST} \
				--output="${LOG_DIR}/slurm-%j.log" \
				--job-name="${JOB_FULL_NAME}" \
				--nodes="${N_NODES}" \
				--nodelist="${NODELIST}" \
				--partition="${SBATCH_PARTITION}" \
				--time="${JOB_TIME}" \
				${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
				"${JOB_FULL_NAME}" \
				"${TEST_NODE}" \
				"${BACKEND_NODE}" \
				"${@[4,-1]}"
		else
			# Launch without sbatch
			${AURORA_TESTS_DIR}/${JOB_SCRIPT} \
				"${JOB_FULL_NAME}" \
				"${TEST_NODE}" \
				"${BACKEND_NODE}" \
				"${@[4,-1]}"
		fi
	done
}

# submit_test 'test script name.zsh' 'hh:mm:ss' \
#     'test name' \
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
			"${TEST_N_RUNS}" \
			"$i" \
			'64' \
			'16'

		submit_test 'heat_distribution_aurora.zsh' '08:00:00' \
			'heatdis_aurora' \
			"${TEST_N_RUNS}" \
			"$i" \
			'128' \
			'16'
	fi

	# Run VeloC Tests
	if [[ $RUN_HEATDIS_VELOC == "true" ]]; then
		submit_test 'heat_distribution_veloc.zsh' '08:00:00' \
			'heatdis_veloc' \
			"${TEST_N_RUNS}" \
			"$i" \
			'64'

		submit_test 'heat_distribution_veloc.zsh' '08:00:00' \
			'heatdis_veloc' \
			"${TEST_N_RUNS}" \
			"$i" \
			'128'
	fi

	# Run Original (unmodified) heatdis benchmark
	if [[ $RUN_HEATDIS_ORIGINAL == "true" ]]; then
		submit_test 'heat_distribution.zsh' '08:00:00' \
			'heatdis_orig' \
			"${TEST_N_RUNS}" \
			"$i" \
			'64'

		submit_test 'heat_distribution.zsh' '08:00:00' \
			'heatdis_orig' \
			"${TEST_N_RUNS}" \
			"$i" \
			'64'
	fi
done

# Heatmap Tests
if [[ $RUN_BLOCK_AURORA == "true" ]]; then
	submit_test 'block_test_aurora.zsh' '4:00:00' \
		'block_test_aurora_heatmap' \
		"${TEST_N_RUNS}" \
		"${HEATMAP_MEM}" \
		"${HEATMAP_PROCS}" \
		'8'

	# submit_test 'block_test_aurora.zsh' '4:00:00' \
	# 	'block_test_aurora_heatmap' \
	# 	"${TEST_N_RUNS}" \
	# 	"${HEATMAP_MEM}" \
	# 	"${HEATMAP_PROCS}" \
	# 	'16'
	#
	# submit_test 'block_test_aurora.zsh' '4:00:00' \
	# 	'block_test_aurora_heatmap' \
	# 	"${TEST_N_RUNS}" \
	# 	"${HEATMAP_MEM}" \
	# 	"${HEATMAP_PROCS}" \
	# 	'32'
fi
