# Building AURORA

General build guide: how to compile from source with configurable paths.

**Critical first step**: Read [`sandbox.md`](./sandbox.md) for container environments where dependencies are wiped on restart. On native systems, check your toolchain and MPI setup first.

---

## Prerequisites & Dependencies

### UCX Version Check
```bash
ucx_info -v                       # Should show UCX 1.17+
ldconfig -p | grep libucp         # Verify in /usr/local/lib, /usr/lib, or /lib64
```

AURORA requires UCX 1.17+ for the `UCP_AM_SEND_FLAG_COPY_HEADER` flag.

### MPI Requirements
MPI is **not required** to build the code, but if available it should be used for parallel tests and better performance. OpenMPI is prefered (do not use MPICH or Intel MPI).
MPI may be installed as a module and not on the default path.
If MPI is installed as a module (e.g., with LMOD), load the OpenMPI module first before building.
Check LMOD before determining that MPI is not installed.

```bash
module avail | grep openmpi       # Check LMOD first
mpicc --show-config               # Verify MPI installation
```

---

## Environment Setup

### Using LMOD Module Systems (if available) to load MPI
LMOD may need to be sourced to work properly. Do not give up if it's not found on the first try.
Sourcing `~/.bashrc` or `~/.zshrc` may help to load lmod. On some RHEL systems, lmod requires sourcing its configuration.

```bash
source ~/.bashrc # or zshrc
# Check for OpenMPI module (if available)
module avail | grep -i openmpi || echo "No OpenMPI modules found"

# If modules are available, load OpenMPI first
module use /apps/modules          # or your modules path
module load mpi/openmpi-x86_64         # adjust version as needed

# Verify MPI is loaded correctly
mpicc --show-config | head -5
which mpicc mpicxx mpirun
```

### Check Toolchain
```bash
which gcc g++ make cmake
mpicc --version                   # MPI compiler version (if available)
```

### Provisioning Notes
Provisioning (compiler, UCX, MPI) is covered in [`sandbox.md`](./sandbox.md) for container environments. On native systems, use the checks above to verify dependencies before building.

---

## Repository Ownership & Build Location

### Locate the Repository
The repository can be at any path. Examples: `~/AURORA`, `/home/user/AURORA`, a bind mount, or similar.

```bash
cd <repo_root>  # e.g., ~/AURORA, wherever you cloned it
git -c safe.directory=$(pwd) log --oneline -1
git -c safe.directory=$(pwd) status --short
```

If Git complains about ownership, run:
```bash
git config --global --add safe.directory $(pwd)
```

### Check Repository Ownership
Determine whether you own the repository by comparing the repo owner with your current user:
```bash
stat -c '%U' <repo_root>/         # Should match: $(whoami)
whoami                            # Your current user
```

### Build Location Rules

- **If you own the repository (same UID)**:  
  Build directly into `$(pwd)/build`. No need to specify `BUILD_DIR` to the Makefile (it's the default), and no need for `UCX_HOME` if host-based UCX is fine.
  ```bash
  cd <repo_root>
  make all
  ```

- **If you don't own the repository** (containers, bind mounts, root-owned):  
  Specify a separate build directory like `~/build/aurora`, `/tmp/aurora-build`, or any writable path.
  ```bash
  cd <repo_root>
  UCX_HOME=/usr/local make BUILD_DIR=~/build/aurora all
  # or for ephemeral: UCX_HOME=/usr/local make BUILD_DIR=/tmp/aurora-build all
  ```

Specify `UCX_HOME=/usr/local` only if the toolchain cannot find UCX automatically (e.g., non-standard installation path).

### Common Build Locations

| Scenario | Recommended Build Directory |
|---|---|
| Native systems (own repo) | `$(pwd)/build` (no BUILD_DIR or UCX_HOME needed) |
| Native systems (don't own repo) | `~/build/aurora`, `~/build-output` |
| Ephemeral containers | `/tmp/aurora-build` or `/tmp/opencode/aurora-build` |
| Any absolute path | Any writable directory for the build user |

---

## Build Commands

### Standard Build (with tests)
```bash
cd <repo_root>  # e.g., ~/AURORA, /home/user/AURORA
make BUILD_DIR=~/build/aurora all
```

**Notes:**
- No need to specify `BUILD_DIR` when building into `$(pwd)/build` (it's the default)
- No need to specify `UCX_HOME` if host-based UCX is found automatically (most common case)
- MPI is not required but should be used if available (see instructions on locating MPI)

### Optimized Build
```bash
make BUILD_DIR=<BUILD_DIR> -DBUILD_TYPE=Release all
```

### Clean Rebuild (from scratch)
```bash
make BUILD_DIR=<BUILD_DIR> clean all
```
This wipes only the build directory, not your source tree.

### Build Without Tests
The default target compiles tests. To skip them (e.g., if MPI is missing or not desired):
```bash
cd <repo_root>
cmake \
    -DCMAKE_TOOLCHAIN_FILE=cmake/host-linux-gnu-toolchain.cmake \
    -B<build_dir> \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_TESTING=OFF \
    -G "Unix Makefiles"
make -C <build_dir> -j"$(nproc)"
```

Replace `<repo_root>` and `<build_dir>` with your actual paths.

---

## Verifying a Build

### Check Configure Output
```bash
grep -E "UCX_FOUND|MPI_C_COMPILER:" <build_dir>/CMakeCache.txt
```

Expected: `UCX_FOUND=1`. `MPI_C_COMPILER` will be set if MPI was found and used, empty otherwise.

### Verify Runtime Library Linking
```bash
ldd <build_dir>/server/aurora_remote_engine | grep libuc
# Should show UCX at common locations: /usr/local/lib, /usr/lib, /lib64
```

### List Test Binaries (if compiled with tests)
```bash
find <build_dir>/tests -maxdepth 1 -type f -perm -u+x
```

A healthy configure prints:
- `Found UCX through CMake`
- `Found MPI with CMAKE` (or `Failed to find MPI` if MPI was not available/loaded)
- `Failed to find VELOC` (warning, benign if VeLOC not installed)

---

## Running Artifacts

### Starting the Remote Engine
```bash
./<build_dir>/server/aurora_remote_engine <log_file_path> &
```

Note: `argv[1]` is a **log file path**, not a flag. Running with `--help` creates a file named literally `--help`.

### MPI Limitations
- In some environments (e.g., containers without proper network interfaces), `mpirun` may hang
- MPI-based tests (`test_discovery_mpi`, `block_test_aurora`, `heatdis_*`) may not run without additional configuration
- This does not affect single-process builds or non-MPI tests

### RDMA Hardware
AURORA's asynchronous progress path requires UCX-compatible RDMA hardware. Builds work without it, but async-progress features won't engage.

---

## Troubleshooting

### "Failed to find UCX" from `FindUCX.cmake`
**Cause**: Distorted multiarch search or missing UCX headers.

**Solutions**:
1. Ensure one of your UCX install paths is used: `/usr/local`, `/usr`, `/lib64`
2. If using distro-packaged UCX, add: `-DCMAKE_LIBRARY_ARCHITECTURE=x86_64-linux-gnu`
3. Check that `libucp.so.*` exists at your UCX location and is visible to `ldconfig`

### "error: 'UCP_AM_SEND_FLAG_COPY_HEADER' undeclared"
**Cause**: UCX version < 1.17.0. AURORA requires UCX 1.17+ for this flag.

**Fix**: Rebuild UCX from source (see [`sandbox.md`](./sandbox.md) Step 2).

### CMake Refuses to Reuse Existing Cache
**Cause**: `CMAKE_HOME_DIRECTORY` in cache points to a different repo path.

**Fix**: Clean and rebuild:
```bash
make BUILD_DIR=<your_build_dir> clean all
# Optional: Add UCX_HOME=<ucx_prefix> only if toolchain can't find UCX automatically
```

### Linker Error with MPI Tests (libucp not found)
**Cause**: UCX library path mismatch or multiarch search suppression in cross mode.

**Fix**:
1. Verify UCX exists at one of your common locations: `/usr/local/lib`, `/usr/lib`, `/lib64`
2. Run `ldd <artifact> | grep libucp` to confirm correct linkage
3. If using distro-packaged UCX, purge dev headers: `sudo apt-get purge -y libucx-dev`

### "dubious ownership" from Git
**Cause**: Repository owned by root but working as non-root user.

**Fix**:
```bash
git config --global --add safe.directory $(pwd)
```

Or adjust permissions (containers may require this):
```bash
sudo chmod -R a+rwX <repo_root>
```

---

## Etiquette

- Do not modify `results/` or `checkpoints/`
- Check `git status` before committing; the repo may have uncommitted changes from the developer's side.
- Use `grep`/`glob` over reading whole files; exclude `results/`, `checkpoints/` from scans.
- Keep generated artifacts in your configured `BUILD_DIR`.

---

## Container-Specific Notes

If you're running inside a container/sandbox:

- `/tmp` is ephemeral — use it for `BUILD_DIR` and temporary tool downloads
- `/usr/local` is typically wiped on restart — rebuild UCX from scratch as needed
- The repo location may vary (`/mnt/repo`, `/workspace/AURORA`, bind mounts, etc.)
- Always configure paths explicitly rather than relying on defaults

For container re-provisioning scripts and minimal setup instructions, see [`sandbox.md`](./sandbox.md).
