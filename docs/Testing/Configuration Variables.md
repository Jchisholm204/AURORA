# Configuration Variables Reference

This document provides a comprehensive reference of all configuration variables used in AURORA's testing infrastructure. Variables are categorized by their scope: **global**, **cluster-specific**, **build-time**, and **runtime**.

---

## Global Environment Variables

Set in [`scripts/env.sh`](env.sh). These define the root directory structure and test harness behavior.

### `AURORA_TOP_DIR`

- **Type**: String (absolute path)
- **Default**: Output of `_ath_finddir_basedir()` (git repo root, or `$(pwd)` if not a git repo)
- **Description**: Root directory of the AURORA repository. All other paths are relative to this.
- **Set via**: Automated by `env.sh`

---

### `AURORA_SCRIPT_DIR`

- **Type**: String (absolute path)
- **Default**: `${AURORA_TOP_DIR}/scripts`
- **Description**: Directory containing test scripts, launchers, and cluster configs.
- **Used by**: All test scripts via `${AURORA_SCRIPT_DIR}`

---

### `AURORA_TESTS_DIR`

- **Type**: String (absolute path)
- **Default**: `${AURORA_SCRIPT_DIR}/tests`
- **Description**: Subdirectory containing individual test driver scripts.
- **Used by**: Test execution scripts

---

### `AURORA_LAUNCH_DIR`

- **Type**: String (absolute path)
- **Default**: `${AURORA_SCRIPT_DIR}/launch`
- **Description**: Directory containing build and launch scripts (`build.zsh`, `launch_test_*.zsh`).

---

### `AURORA_CLUSTER_DIR`

- **Type**: String (absolute path)
- **Default**: `${AURORA_SCRIPT_DIR}/clusters/${AURORA_CLUSTER_NAME}`
- **Description**: Cluster-specific configuration directory.
- **Used by**: Environment sourcing in test scripts

---

### `AURORA_LOG_DIR`

- **Type**: String (relative or absolute path)
- **Default**: `results/0.0.3`
- **Description**: Base directory for all log and result files. If relative, resolved from `${AURORA_TOP_DIR}`.
- **Note**: Directory is created automatically if it doesn't exist

---

### `AURORA_CLUSTER_NAME`

- **Type**: String
- **Default**: `rome`
- **Description**: Cluster identifier (folder name under `scripts/clusters/`). Must match a folder in that directory.
- **Example Values**: `rome`, `odp0k`, `testbed`, `production`

---

### `AURORA_CLUSTER_NODES_STR`

- **Type**: String (comma-delimited)
- **Default**: Not set by default; populated from cluster config
- **Description**: Comma-separated list of all cluster nodes for easy access in scripts.
- **Derived from**: `${AURORA_CLUSTER_NODES}` array via `join-by-comma`

---

### `AURORA_CLUSTER_BACKEND_NODES`

- **Type**: String (comma-delimited)
- **Default**: Not set by default; populated from cluster config
- **Description**: Backend nodes only (excludes test nodes). Used for launching the server.
- **Derived from**: `${AURORA_CLUSTER_NODES[@]#*,}` (strip first element of each pair)

---

### `AURORA_CLUSTER_TEST_NODES`

- **Type**: String (comma-delimited)
- **Default**: Not set by default; populated from cluster config
- **Description**: Test nodes only (excludes backend nodes). Used for launching the client.
- **Derived from**: `${AURORA_CLUSTER_NODES[@]%%,*}` (strip second element of each pair)

---

### `SBATCH_PARTITION` / `SLURM_PARTITION`

- **Type**: String
- **Default**: Not set; must be configured per cluster
- **Description**: Slurm partition name for job submission. Both variables are exported for compatibility with older scripts.

---

### `AURORA_EXPORT_LIST`

- **Type**: Zsh array
- **Default**: Populated by `env.sh`
- **Elements**: List of all exported variables (for use with `--export=` in sbatch)

---

## Cluster-Specific Variables

Set in [`scripts/clusters/<CLUSTER_NAME>/launch_config.sh`](clusters/rome/launch_config.sh). These define cluster topology and platform.

### `AURORA_CLUSTER_NODES`

- **Type**: Zsh array
- **Description**: Array of node-hostname pairs. Order matters: `[test_node,backend_node]` per entry.
- **Example**:
  ```zsh
  export AURORA_CLUSTER_NODES=(
      "rome001,romebf2a001"
      "rome002,romebf2a002"
  )
  ```
- **For non-BF clusters**: Single-element arrays (e.g., `"node001"`)

---

### `AURORA_LAUNCH_PLATFORM`

- **Type**: String
- **Valid Values**: `host`, `slurm`, `ssh`, `none`
- **Description**: Single switch that selects how both the backend and the client are launched.
  - `host`: local server and local `mpirun` (single-node entries)
  - `slurm`: server via `srun` on the backend node of a pair, client via `mpirun`
  - `ssh`: one server per backend node started over SSH, client via `mpirun`
  - `none`: external/persistent server; client only

`slurm` and `ssh` require `"client,backend"` pairs in `AURORA_CLUSTER_NODES`.

---

### `AURORA_SSH_USER` / `AURORA_SSH_OPTS` / `AURORA_REMOTE_ENV`

- **Type**: String (optional)
- **Description**: SSH launch overrides.
  - `AURORA_SSH_USER`: prepended as `user@` to the backend host. Leave unset for
    userless SSH configured by the cluster admin.
  - `AURORA_SSH_OPTS`: extra `ssh` options, word-split (for example
    `-o BatchMode=yes`).
  - `AURORA_REMOTE_ENV`: optional script sourced on the backend node before
    starting the server.

---

### `AURORA_CLUSTER_CHECKPOINT_DIR`

- **Type**: String (absolute path, no trailing `/`)
- **Description**: Directory for storing checkpoint data. Must be accessible across all nodes.

---

### `AURORA_CLUSTER_TMP_DIR`

- **Type**: String (absolute path, no trailing `/`)
- **Description**: Temporary directory for build artifacts and intermediate files. Must have sufficient space for builds.

---

## Build-Time CMake Variables

Passed to `cmake()` during the build step (via [`scripts/launch/build.zsh`](launch/build.zsh)).

### `BUILD_TESTING`

- **Type**: Boolean (`ON` / `OFF`)
- **Default**: `ON` in Makefile, `OFF` in `build.zsh` unless specified
- **Description**: Enable compilation of test executables. Set to `OFF` to skip tests (e.g., when MPI is unavailable).

---

### `ACR_MAX_WORKERS`

- **Type**: Integer
- **Default**: Not set; uses default from `server/src/are.c`
- **Description**: Maximum number of worker threads in the server's dispatch thread pool.
- **Recommended**: `$=2 × <number of MPI ranks>`
- **Example**: `-DACR_MAX_WORKERS=16`

---

### `AIM_MAX_WORKERS`

- **Type**: Integer
- **Default**: Not set; uses default from `server/src/are.c`
- **Description**: Maximum number of worker threads for server connections (AIM).
- **Default Value**: 256 connections
- **Example**: `-DAIM_MAX_WORKERS=256`

---

### `AUL_DEFAULT_RESTART_REMOTE`

- **Type**: Boolean (`ON` / `OFF`)
- **Default**: `OFF`
- **Description**: Build AURORA clients with DPU-offloaded restore. Equivalent to defining `AUL_DEFAULT_RESTART_REMOTE` before including `aul.h`; see [Client Lib](../../Client%20Lib/Client%20Lib.md).
- **Example**: `-DAUL_DEFAULT_RESTART_REMOTE=ON`

---

### `VELOC_INSTALL_DIR`

- **Type**: String (absolute path, optional)
- **Default**: Auto-detected via `find_package(veloc)`
- **Description**: Override the default VeLOC installation directory. Required if `find_package()` fails to locate it.
- **Example**: `-DVELOC_INSTALL_DIR=/path/to/veloc`

---

## Runtime Test Parameters

Passed directly to test executables or set via script arguments. These vary by test type.

### General Parameters (All Tests)

| Variable | Type | Description | Example Values |
|----------|------|-------------|-----------------|
| `JOB_NAME` | String | Job identifier for logging and cleanup | `heatdis_aurora_mem64_p64_i1` |
| `MEM_MB` | Integer | Memory size in MB for checkpoint tests | `64`, `128`, `256`, `512`, `1024`, `16384` |
| `PROCS` | Integer | Number of MPI processes (client side) | `64`, `128` |
| `N_RUNS` | Integer | Number of iterations to run per condition | `5`, `10` |

---

### Backend-Specific Parameters

| Variable | Type | Description | Example Values |
|----------|------|-------------|-----------------|
| `BACKEND_PROCS` | Integer | Number of backend server threads | `8`, `16`, `32` |

---

### Block Test Specific

| Variable | Type | Description | Example Values |
|----------|------|-------------|-----------------|
| `CHECKPOINT_DIR` | String | Directory for checkpoint data | `${AURORA_CLUSTER_CHECKPOINT_DIR}/<job_name>_...` |
| `TMP_DIR` | String | Temporary directory for builds and intermediates | `${AURORA_CLUSTER_TMP_DIR}/<job_name>/build` |

---

### VeLOC Test Specific

| Variable | Type | Description | Example Values |
|----------|------|-------------|-----------------|
| `TMP_CHECKPOINT_DIR` | String | Temporary checkpoint directory (scratch) | Same as `${CHECKPOINT_DIR}` with `_tmp_checkpoints` suffix |
| `TMP_META_DIR` | String | Temporary metadata directory for VeLOC | `/tmp/<job_name>_..._meta` |

---

## Directory Structure Variables

### `results/`

- **Location**: `${AURORA_TOP_DIR}/results/${AURORA_LOG_DIR}`
- **Purpose**: All test logs, CSV outputs, and plot files
- **Subdirectories** (auto-created):
  - `<test_name>/<iteration>` — Per-test run results
  - `block_test_aurora_heatmap` — Block test heatmap data
  - `heatdis_stats/` — Statistical comparison outputs
  - `heatmaps/` — Generated plot images

---

### `checkpoints/`

- **Location**: `${AURORA_CLUSTER_CHECKPOINT_DIR}`
- **Purpose**: Persistent checkpoint storage (not included in git)
- **Note**: This directory grows rapidly; use with appropriate storage quotas

---

## Build Directory Variables

### `TEST_BUILD_DIR`

- **Description**: Build directory for test executables (x86_64)
- **Default**: `${BUILD_DIR}_test`
- **Contains**: Test client programs

---

### `BACKEND_BUILD_DIR`

- **Description**: Build directory for backend server executables (aarch64)
- **Default**: `${BUILD_DIR}_backend`
- **Contains**: Server executable (`aurora_remote_engine`)

---

## See Also

- [Generic Testing Scripts](Generic%20Testing%20Scripts.md)
- [Cluster Specific Configuration](Cluster%20Specific%20Configuration.md)
- [Adding New Clusters](Adding%20New%20Clusters.md)
- [Collecting Results](Collecting%20Results.md)
