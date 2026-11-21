# Adding New Clusters

This document explains how to configure and add a new cluster for AURORA testing. Each cluster requires its own directory under `scripts/clusters/` with two configuration scripts.

---

## Directory Structure

Create a new directory:

```bash
mkdir -p scripts/clusters/<CLUSTER_NAME>
```

Replace `<CLUSTER_NAME>` with your cluster identifier (e.g., `odp0k`, `rome`, `testbed`).

The directory must contain exactly two files:
- `env.sh` — Module loading and environment setup
- `launch_config.sh` — Node configuration and platform settings

---

## `env.sh` — Environment Setup

### Purpose

The `env.sh` script sets up the compiler, dependencies, and optional cross-compilation toolchains for building AURORA on the cluster.

### Required Modules

Your script must load at minimum:
- **CMake** — Build system
- **GCC** — Compiler (version 11 or compatible)
- **HPCX / UCX** — UCX libraries (`hpcx/2.20` for UCX 1.17.0, or equivalent)
- **MPI** — Message passing interface (`openmpi`, `mpich`, etc.)

Optional but recommended:
- **VeLOC** — If you plan to run VeLOC benchmark tests

### Architecture Support

The script should support cross-compilation for BlueField ARM devices:

1. Accept the architecture as the first argument (`$1`)
2. Default to local architecture if no argument is given
3. Validate against supported options (`aarch64`, `x86_64`, `none`)

Example structure:

```zsh
#!/usr/bin/env zsh

# Arch Independent Environment Setup Script

local ARCH_OPTIONS=( "aarch64" "x86_64" "none" )

# Setup local or cross compilation env
function setup_env() {
    local ARCH=$1
    local LOCAL_ARCH=$(uname -m)

    echo "Setting up $ARCH environment for $LOCAL_ARCH"

    # Restore/Load Local ENVs
    source /etc/profile.d/modules.sh
    export MODULEPATH="/global/software/rocky-9.$LOCAL_ARCH/modfiles/langs:/global/software/rocky-9.$LOCAL_ARCH/modfiles/tools:/global/software/rocky-9.$LOCAL_ARCH/modfiles/apps"

    module purge

    module load cmake

    export MODULEPATH="/global/software/rocky-9.$ARCH/modfiles/langs:/global/software/rocky-9.$ARCH/modfiles/tools:/global/software/rocky-9.$ARCH/modfiles/apps"

    module load gcc/11
    module load hpcx/2.20

    # Optional: Load cross-compilation tools for aarch64
    if [[ $ARCH != $LOCAL_ARCH && $ARCH == "aarch64" ]]; then
        echo "Loading Cross Compilation Tools"
        module use $HOME/.modules/modfiles
        module load gcc-arm/15.2
    fi

    # Optional: Load VeLOC for x86_64 builds
    if [[ "${ARCH}" == 'x86_64' ]]; then
        echo "Loading VeloC"
        module use $HOME/.modules/modfiles
        module load veloc
    fi
    return 0
}

# Default Option - Use Host Arch
local ARCH=$(uname -m)

if [[ $1 ]]; then
    # Check 
    if [[ ${ARCH_OPTIONS[(r)$1]} == $1 ]]; then
        ARCH=$1
    else
        echo "Invalid Arch Option: $1"
        echo "Valid Options = {$ARCH_OPTIONS}"
        return -1
    fi
fi

if [[ $ARCH == "none" ]]; then
    return 0
else
    setup_env $ARCH
fi
```

---

## `launch_config.sh` — Cluster Configuration

### Purpose

The `launch_config.sh` script exports the cluster-specific variables used by test scripts to launch jobs on your cluster.

### Required Variables

#### `SBATCH_PARTITION` / `SLURM_PARTITION`

Slurm partition name for job submission:

```zsh
export SBATCH_PARTITION=<partition_name>
export SLURM_PARTITION=$SBATCH_PARTITION
```

Example: `rome`, `compute`, `batch`

---

#### `AURORA_CLUSTER_NODES`

Array of node-BlueField (or host) hostname pairs. Order matters:

```zsh
export AURORA_CLUSTER_NODES=(
    # First element = test node (x86_64 where client runs)
    # Second element = backend node (aarch64 BF device, if using BlueField)
    "node001,bf001"
    "node002,bf002"
    ...
)
```

**For clusters without BlueField:**

```zsh
export AURORA_CLUSTER_NODES=(
    "node001"
    "node002"
    ...
)
```

Each entry corresponds to a Slurm job node. Names must match actual Slurm hostnames.

---

#### `AURORA_BACKEND_PLATFORM`

Determines the server launch mechanism:

```zsh
export AURORA_BACKEND_PLATFORM="none"  # External backend, no server launch
# or
export AURORA_BACKEND_PLATFORM="bf"    # BlueField ARM backend (launch via srun)
# or
export AURORA_BACKEND_PLATFORM="host"   # Host-based emulation (single x86_64 node)
```

**Important**: Only one platform can be specified at a time. The string is case-sensitive.

| Platform | Description | Use Case |
|----------|-------------|----------|
| `none` | External backend | When server runs separately; tests only run on x86_64 nodes |
| `bf` | BlueField ARM | When testing on clusters with BF2/BF3 DPU nodes |
| `host` | Host emulation | Development mode; both test and backend on same architecture |

---

#### `AURORA_CLUSTER_CHECKPOINT_DIR`

Absolute path to the checkpoint directory (do not include trailing `/`):

```zsh
export AURORA_CLUSTER_CHECKPOINT_DIR="/path/to/checkpoints"
```

This is where AURORA stores checkpoint data during testing.

---

#### `AURORA_CLUSTER_TMP_DIR`

Absolute path for temporary files, including build artifacts (do not include trailing `/`):

```zsh
export AURORA_CLUSTER_TMP_DIR="/tmp/path/to/builds"
```

Build directories are created under this path (e.g., `${TMP_DIR}/build/test`, `${TMP_DIR}/build/backend`).

---

## Example: Complete `rome/` Cluster Configuration

```zsh
#!/usr/bin/env zsh

# ROME specific test configuration script

# Slurm Extra Options
export SBATCH_PARTITION=rome
export SLURM_PARTITION=$SBATCH_PARTITION

# Export Possible Launch Pair Configurations
# Each should be "node-hostname,bf-hostname"
export AURORA_CLUSTER_NODES=(
    # Uncomment to use BF2 nodes
    # "rome001,romebf2a001"
    # "rome002,romebf2a002"
    
    # Uncomment to use BF3 nodes
    "rome005,romebf3a005"
)

# Set the backend platform (bf (for arm devices), host, none)
export AURORA_BACKEND_PLATFORM="bf"

# Set the checkpoint directory to use on this cluster
export AURORA_CLUSTER_CHECKPOINT_DIR="$HOME/exafs/checkpoints"

# Temporary directory to use during testing
export AURORA_CLUSTER_TMP_DIR="$HOME/exafs/tmp/checkpoints"
```

---

## Validation Checklist

Before adding a new cluster, ensure:

- [ ] `env.sh` loads all required modules (CMake, GCC, MPI, UCX)
- [ ] `env.sh` supports architecture argument (`x86_64`, `aarch64`, `none`)
- [ ] `launch_config.sh` defines `AURORA_CLUSTER_NODES` array
- [ ] `launch_config.sh` sets `AURORA_BACKEND_PLATFORM` to a valid value
- [ ] Both checkpoint and tmp directories use absolute paths without trailing `/`
- [ ] Directory structure matches: `scripts/clusters/<CLUSTER_NAME>/`
- [ ] Both scripts have execute permissions (`chmod +x`)

---

## Testing Your Cluster Configuration

After creating the cluster config, validate it before using in tests:

```zsh
# Test env.sh
source scripts/clusters/<CLUSTER_NAME>/env.sh 'x86_64'

# Test launch_config.sh
source scripts/clusters/<CLUSTER_NAME>/launch_config.sh

# Verify exports
echo "Cluster Nodes: ${AURORA_CLUSTER_NODES[@]}"
echo "Backend Platform: $AURORA_BACKEND_PLATFORM"
echo "Checkpoint Dir: $AURORA_CLUSTER_CHECKPOINT_DIR"
echo "Tmp Dir: $AURORA_CLUSTER_TMP_DIR"
```

---

## See Also

- [Generic Testing Scripts](Generic%20Testing%20Scripts.md)
- [Cluster Specific Configuration](Cluster%20Specific%20Configuration.md)
- [Running Tests](Running%20Tests.md)
- [Configuration Variables](Configuration%20Variables.md)
