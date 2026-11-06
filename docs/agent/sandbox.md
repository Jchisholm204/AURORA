# Agent Sandbox Environment

Build guide for ephemeral containerized environments (Debian-based, bookworm) where dependencies are typically wiped on restart.

**Important**: This is specific to Debian containers. For other OS versions or native builds, check what's already installed first and adapt paths accordingly.

---

## Environment Overview

| Property | Container Value |
|---|---|
| Base OS | Debian GNU/Linux 12 (bookworm) |
| Kernel | Host kernel typically used via `odp0k` module |
| User | `coder` (uid 1000), with `sudo` = `(ALL) NOPASSWD: ALL` |
| Seccomp | Enabled, `CapEff: 0` (privileges from sudo, not caps) |
| Network | Outbound HTTPS + DNS available |
| RDMA hardware | None expected (UCX runs without async-progress path) |

### Ephemeral Nature

The container is reset on restart. The following are **lost**:

- Everything in `/tmp` (including build artifacts)
- Everything installed into `/usr/local` (including from-source UCX)
- All `apt` packages

**Survives**:
- Repository at your specified path (bind-mounted or mutagen-synced)

### Consequence

Provisioning must be repeated each session. Bake steps into the image if this becomes routine.

---

## Environment Check (Run First!)

Before installing anything, check what's already available:

```bash
# Compiler and toolchain
which gcc g++ make cmake || echo "Missing: one or more of gcc, g++, make, cmake"

# Module support check (if using LMOD) - CHECK THIS FIRST for MPI
module 2>/dev/null | grep -q "^module" && echo "LMOD found in PATH" || echo "No modules in PATH"

# If LMOD is available, check for OpenMPI module before searching PATH
if command -v module &>/dev/null; then
    module avail | grep -q openmpi && echo "OpenMPI available via LMOD" || echo "No OpenMPI in LMOD"
fi

# UCX version check - MUST DO THIS FIRST (Debian ships old UCX)
ucx_info -v 2>/dev/null && echo "UCX found, checking version..." || echo "UCX not found in PATH"
ldconfig -p | grep libucp || echo "No UCX library visible to ldconfig"

# MPI status (via module or direct)
mpicc --version 2>/dev/null && echo "MPI compiler available" || echo "MPI compiler (mpicc) not found"
which mpirun || echo "mpirun not found or in PATH"
```

**If all tools exist**: Skip installation and proceed directly to build.

**Module-based MPI note:** If `module` is in PATH but MPI isn't loaded, run the module commands above before building.

**Debian/Ubuntu UCX note:** The distro ships UCX 1.13.x by default, which AURORA requires UCX 1.17+. Always check version first with `ucx_info -v` before deciding to install from source.

---

## Step 1 — Toolchain and Dependencies

Only run if `gcc`, `g++`, `make`, and `cmake` are missing (check with `which` above).

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    gcc g++ make libc6-dev cmake pkg-config \
    openmpi-bin libopenmpi-dev \
    libibverbs-dev librdmacm-dev libnuma-dev
```

### Why These Packages?

- **`libc6-dev`**: Must be explicit. With `--no-install-recommends`, `gcc` alone does NOT pull it in, and compilation fails on `#include <sys/socket.h>`.
- **`g++`**: Required even though AURORA is pure C (0 `.cpp` files). CMakeLists.txt enables both C and CXX; the toolchain hardcodes `CMAKE_CXX_COMPILER=g++` which cannot be overridden.
- **`openmpi-bin`**: Pulls in `libucx0` (1.13.1) as a hard dependency. See UCX section — removing it is bad.

### Versions Resolved

GCC 12.2.0, CMake 3.25.1, OpenMPI 4.1.4.

---

## MPI Module Support (if available)

If MPI is installed via modules (e.g., LMOD), load OpenMPI before building:

```bash
module use /apps/modules           # or your module path
module avail | grep openmpi        # list available versions
module swap tcl tcsh               # optional, for interactive shells
module load openmpi/4.1.4          # adjust version as needed
```

**Important:** OpenMPI is preferred over MPICH or Intel MPI for AURORA builds.

After loading, verify with:
```bash
mpicc --show-config | head -5
which mpicc mpicxx mpirun
```

If modules are unavailable or you installed MPI directly, skip this step and proceed to UCX installation.

---

## Step 2 — UCX Installation (Debian-Specific)

### Background: Version Requirements

Debian bookworm ships UCX **1.13.1** via `libucx0`. AURORA uses `UCP_AM_SEND_FLAG_COPY_HEADER` (`common/src/arm/arm_rm.c`), which does not exist in 1.13.1.

The build fails with:
```
error: 'UCP_AM_SEND_FLAG_COPY_HEADER' undeclared
```

**AURORA targets UCX 1.17.0 / HPCX 2.20**. If you need to use VeLOC (if installed), it also requires UCX 1.17+.

### Check What's Installed First

```bash
ucx_info -v 2>/dev/null || echo "UCX not installed"
ldconfig -p | grep libucp
which mpicc mpicxx
```

If UCX is missing or too old, install from source.

### Install UCX from Source (into any prefix)

Default: `/usr/local`, but can be `~/local`, `/opt/ucx-1.17`, etc.

```bash
mkdir -p ~/opencode  # or /tmp/opencode in ephemeral containers; any writable location works
cd ~/opencode        # change to your temp work directory

# Check for MPI modules first (if using LMOD)
module avail | grep openmpi || echo "No OpenMPI modules found"

curl -sSLO https://github.com/openucx/ucx/releases/download/v1.17.0/ucx-1.17.0.tar.gz
tar xzf ucx-1.17.0.tar.gz
cd ucx-1.17.0

./configure --prefix=/usr/local \
    --enable-mt --with-verbs --with-rdmacm --with-numa --enable-devel-headers
make -j"$(nproc)"
sudo make install
sudo ldconfig
```

### Verify Installation

```bash
ucx_info -v | head -1           # Should show UCX 1.17.x
ldconfig -p | grep libucp       # Should show your UCX prefix/lib/libucp.so.*
```

### Handling Distro-Packaged UCX Conflicts

If `libucx0` (1.13.1) is installed and conflicts with your source build, purge the dev headers:

```bash
sudo apt-get purge -y libucx-dev ucx-utils 2>/dev/null || true
```

This avoids header shadow conflicts while preserving runtime compatibility via `/usr/local/lib`.

### Custom UCX Prefix (Optional)

If you prefer not to use `/usr/local`:

1. Change `--prefix` in configure to any writable location (e.g., `~/local`, `/opt/ucx-1.17`)
2. Set `UCX_HOME=<your_prefix>` when building AURORA
3. The toolchain file appends this to `CMAKE_PREFIX_PATH` for UCX CMake config discovery

---

## Step 3 — Build AURORA

### Finding Required Libraries (MPI, UCX)

**Critical: Use LMOD to check available modules first.**

```bash
module avail | grep -i openmpi || echo "No OpenMPI found - install/build needed"
```

### MPI Requirements

- **OpenMPI is required**. Do not use MPICH or Intel MPI.
- **Check with LMOD first** if available (clusters/common environments):
  ```bash
  module avail | grep -i openmpi || echo "No OpenMPI in LMOD - may need direct install"
  ```
- Check with: `mpicc --show-config` (should show OpenMPI paths)

### UCX Version Requirements

AURORA requires UCX 1.17+. Debian bookworm ships UCX 1.13.1, which is incompatible.

**Check installed version FIRST** (before installing):
```bash
ucx_info -v                          # Should show UCX 1.17+
ldconfig -p | grep libucp            # Should show your UCX prefix/lib/libucp.so.*
```

If UCX is missing or too old, proceed to Step 2 installation below.

### Toolchain Check

```bash
which gcc g++ make cmake             # All must exist
mpicc --version                      # MPI compiler version
```

---

### Repository Location

The repository can be at any path:
- `~/AURORA` (default example)
- `/home/user/AURORA`
- Any bind mount or mutagen-synced location

Ensure it's writable if synced from another machine:

```bash
sudo chmod -R a+rwX <repo_path>  # e.g., ~/AURORA, /mnt/repo
git config --global --add safe.directory <repo_path>  # if Git complains
```

### Build Directory Setup

**First, check if you own the repository:**

```bash
ls -ld <repo_root> | awk '{print $3}'    # shows owner (e.g., root, coder)
whoami                                   # your current user
```

#### If You Own the Repository (Same UID)

When the repo owner matches your current user:

```bash
cd <repo_root>  # e.g., ~/AURORA, wherever your repo is located
UCX_HOME=/usr/local make BUILD_DIR=$(pwd)/build clean all
```

You can build directly into `$(pwd)/build`.

#### If You Don't Own the Repository (Different Owner)

When the repo is owned by root or another user (common in containers, bind mounts):

**Never build into the repo's `build/` directory** — it may contain stale caches or paths from another machine.

Use a separate, configurable build directory:

```bash
cd <repo_root>        # e.g., ~/AURORA, wherever your repo is located
UCX_HOME=/usr/local make BUILD_DIR=~/build/aurora clean all
# or: UCX_HOME=/usr/local make BUILD_DIR=/tmp/aurora-build clean all
```

**Path Configuration:**
- **`<repo_root>`**: Your repository path (e.g., `~/AURORA`, `/home/user/AURORA`, bind mounts)
- **`UCX_HOME`**: UCX prefix (default `/usr/local`; can be `~/local`, `/opt/ucx-1.17`, etc.)
- **`BUILD_DIR`**: Build output location

**Examples for different environments:**
```bash
# Native systems (own repo): native paths
UCX_HOME=/usr/local make BUILD_DIR=$(pwd)/build clean all

# Native systems (don't own repo): home directory
UCX_HOME=/usr/local make BUILD_DIR=~/build/aurora clean all

# Ephemeral container: temp paths  
UCX_HOME=/usr/local make BUILD_DIR=/tmp/aurora-build clean all

# Custom UCX prefix
UCX_HOME=/opt/ucx-1.17 make BUILD_DIR=/home/user/build-output clean all
```

**Common Build Locations:**

| Scenario | Recommended Location |
|---|---|
| Own repo (native) | `$(pwd)/build` |
| Don't own repo (native) | `~/build/aurora`, `~/build-output` |
| Ephemeral container | `/tmp/aurora-build`, `/tmp/opencode/aurora-build` |

### Expected Configure Output

```
-- Found UCX though CMake
-- Found MPI with CMAKE
CMake Warning at cmake/FindVeLOC.cmake:16 (message):
  Failed to find VELOC
```

The VeLOC warning is benign — tests for VeLOC are skipped if not installed.

### Build Artifacts

Output locations depend on your `BUILD_DIR`:

```
<build_dir>/common/libaroc.a
<build_dir>/lib/libaul.so
<build_dir>/server/aurora_remote_engine
<build_dir>/tests/<test_name>
<build_dir>/compile_commands.json
```

**Ten test binaries (VeLOC-free)**: `test_ael_server`, `test_ael_client`, `test_afv_checkpoint`, `test_afv_metadata`, `test_afv_restore`, `test_discovery`, `test_discovery_mpi`, `heatdis_aurora`, `heatdis_original`, `block_test_aurora`.

### Incremental vs. Clean Build

```bash
# Incremental (keeps object files)
UCX_HOME=/usr/local make BUILD_DIR=<build_dir> build

# From scratch (wipes only your build directory, not source)
UCX_HOME=/usr/local make BUILD_DIR=<build_dir> clean all
```

If the repo changes between builds (files added/renamed via git), prefer a clean rebuild — stale objects can mask breakage.

---

## Environment Detection Checklist

When starting a fresh session, verify this state:

1. **Tools available**: `which gcc g++ make cmake ucx_info mpicc`
2. **UCX version FIRST** (Debian default is 1.13.x, too old): `ucx_info -v` (should show 1.17+)
3. **Repository accessible**: `git log --oneline -1` in your repo path
4. **Build directory exists and is writable**: `ls -ld <build_dir>`
5. **MPI via LMOD or direct**: 
   - If modules available: `module avail | grep openmpi`
   - Else verify: `mpicc --show-config` shows OpenMPI

If any check fails, install/build as appropriate before running tests.

---

## Pitfalls and Troubleshooting

### Do Not Build Into `build/`

The repo's `build/` directory is meant for native development machines. Its `CMakeCache.txt` may contain:
- `CMAKE_HOME_DIRECTORY` pointing to a different path
- Host-specific MPI paths (e.g., Fedora `/usr/lib64/openmpi/bin/mpicc`)

CMake refuses to reuse such caches, leading to confusing failures. Always override `BUILD_DIR`.

### Multiarch Trap with Distro-Packaged UCX

If UCX is from the distro (`libucx0`), CMake in cross-compilation mode may suppress multiarch library search, causing `find_library()` to fail for `libucp` and `libz`.

**Workaround**: If you must use distro-packaged UCX, add to your build:
```bash
-DCMAKE_LIBRARY_ARCHITECTURE=x86_64-linux-gnu
```

The `/usr/local` source install avoids this entirely (`/usr/local` is always searched).

### MPI Tests May Hang

- `mpirun` hangs in many container environments (no usable TCP interface)
- The five MPI-based tests won't run without additional network/RDMA configuration:
  - `test_discovery_mpi`
  - `block_test_aurora`
  - `heatdis_*` (both variants)

This is expected and does not affect single-process builds.

### No RDMA Hardware

UCX builds without it, but AURORA's asynchronous progress path requires UCX-compatible RDMA hardware and won't engage in its absence.

### Remote Engine Log File Path

`argv[1]` to `aurora_remote_engine` is a **log file path**, not a flag:
- Running `./aurora_remote_engine --help` creates a file named literally `--help`
- Start it as: `UCX_HOME=<ucx_prefix> ./<build_dir>/server/aurora_remote_engine <log_file_path> &`

---

## Minimal Re-provisioning Script

For containers where everything is wiped on restart:

```bash
#!/bin/bash
set -e
export UCX_HOME=/usr/local  # or your custom prefix

# Module support (if available)
if command -v module &>/dev/null; then
    module use /apps/modules 2>/dev/null || true
    if module avail | grep -q openmpi 2>/dev/null; then
        echo "Loading OpenMPI..."
        module load openmpi/4.1.4  # or your version
    fi
fi

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    gcc g++ make libc6-dev cmake pkg-config \
    openmpi-bin libopenmpi-dev \
    libibverbs-dev librdmacm-dev libnuma-dev

# Install UCX from source (into /usr/local or your prefix)
cd ~/opencode  # or your temp work directory
rm -rf ucx-1.17.0 && curl -sSLO https://github.com/openucx/ucx/releases/download/v1.17.0/ucx-1.17.0.tar.gz
tar xzf ucx-1.17.0.tar.gz && cd ucx-1.17.0

# Check available modules before loading (if using LMOD)
module avail | grep openmpi || echo "No OpenMPI modules found - skipping module load"

./configure --prefix=/usr/local --enable-mt \
    --with-verbs --with-rdmacm --with-numa --enable-devel-headers
make -j"$(nproc)" && sudo make install && sudo ldconfig

### Build AURORA

```bash
cd <repo_root>  # e.g., ~/AURORA, wherever your repo is located

# Determine build location based on ownership
BUILD_DIR=$(pwd)/build    # if you own the repo
# or: BUILD_DIR=~/build/aurora  # if you don't own it

UCX_HOME=/usr/local make BUILD_DIR=$BUILD_DIR clean all
```

**Path Configuration:**
- **`<repo_root>`**: Your repository path (e.g., `~/AURORA`, `/home/user/AURORA`)
- **`BUILD_DIR`**: Build output location (see table below)
- **`UCX_HOME`**: UCX prefix (default `/usr/local`; can be `~/local`, `/opt/ucx-1.17`)

**Common Build Locations:**

| Scenario | Recommended Location |
|---|---|
| Own repo (native) | `$(pwd)/build` |
| Don't own repo (native) | `~/build/aurora`, `~/build-output` |
| Ephemeral container | `/tmp/aurora-build`, `/tmp/opencode/aurora-build` |

---

## Path Configuration Reference

When problems arise, check which environment variable needs adjusting:

- **`UCX_HOME`**: Path to UCX installation (default `/usr/local`)
- **`BUILD_DIR`**: Build output location (see table above)
- **Repository location**: Can be any path — bind mount, sync target, or native checkout

The examples throughout these docs use:
- Repository: `~/AURORA` or `<repo_root>`
- Temp work: `~/opencode` or `/tmp/opencode` (ephemeral containers)
- Build output: see **Common Build Locations** table above
- UCX prefix: `/usr/local` (default)

---

## Cross-Reference to Building Guide

For a more general build guide that works on native systems, see [`building.md`](./building.md). It includes:
- **Finding Required Libraries**: Dedicated section about locating MPI, UCX, and toolchain using LMOD
- Build location rules based on repository ownership
- Troubleshooting for common CMake and linking errors
