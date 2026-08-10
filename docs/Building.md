This document details the build instructions for AURORA.

# Dependencies
## External Dependencies
- CMake ($\ge$ v3.21)
- UCX
	- UCT
	- UCS
	- UCP
- Pthreads
- MPI (Optional, Required for tests)
- [VeLOC](https://github.com/ECP-VeloC/VELOC/tree/main) (Optional, Required for VeLOC tests)
## Internal Dependencies
All code dependencies are statically linked and should be pulled in using Git Submodules.
- Log Log (pulled from loglog repo, in common lib)
- MDNS (pulled from openmdns repo, in common lib ADS)
## Hardware Dependencies
UCX compatible RDMA capable hardware is required for asynchronous progress.
Otherwise, asynchronous progress will NOT take place.
This code has been optimized for NVIDIA BlueField-2/3 DPUs, but will run on any UCX capable hardware, including machines without RDMA hardware.

# Building from Source
1. Install Dependencies
2. Examine the [[#Build Options]]
3. [[#Build with CMake (x86_64)]]
4. [[#AArch64 (BlueField) Cross Compilation]]
## Build Options
### Building AURORA Tests
Defaults to `OFF`, set to `ON` to enable compilation of the tests.
VeLOC tests will automatically be build if the VeLOC library can be found.
MPI is required to build the tests.

```
BUILD_TESTING = {ON, OFF}
```
### Server Thread Count Configuration
Configures the number of threads present in the server's dispatch thread pool.
If not set, the default is defined within `server/src/are.c`, currently `16` to match the core count of a BlueField 3.
*Note*; this count does not include the master thread.

```
ACR_MAX_WORKERS
```
### Server Connection Limit Configuration
Configures the maximum number of client connections the server can support simultaneously.
The recommended value is  $=2 \times \text{number of mpi ranks}$.
If not set, the default is defined within `server/src/are.c`, currently `256`.

```
AIM_MAX_WORKERS
```
### Building VeLOC Tests / Install Directory
VeLOC tests will automatically be build if the VeLOC library can be found.
CMake will attempt to find the VeLOC package (`find_package(veloc)`), however the package location can also be specified by setting the following variable:
```
VELOC_INSTALL_DIR=/path/to/veloc_base_dir
```

## Build with CMake (x86_64)
The following CMake command can be used to generate the build system for all AURORA components using the following parameters:
- Build Output Directory: `build`
- CMake Export Compile Commands: `ON` (exports json for clang autocomplete)
- Build Tests: `ON`
- Server Thread Count: 16 Threads
- Server Connection Limit: 256 Connections

```sh
cmake \
	-DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-linux-gnu-toolchain.cmake \
	-Bbuild \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DBUILD_TESTING=ON \
	-DACR_MAX_WORKERS=16 \
	-DAIM_MAX_WORKERS=256 \
	-G "Unix Makefiles" 
```

>[!NOTE]
>The above command can also be executed by running the command `make clean all` from the base repository directory.
>This command uses the top level MakeFile to invoke CMake with the default build settings.

## AArch64 (BlueField) Cross Compilation
> [!NOTE]
> Cross-Compilation is not required to build the BlueField executables.
> With SSH access to a BlueField device, the `host-linux-gnu-toolchain` can be used to build AArch64 executables. (formerly x86_64 toolchain)
> When building, ensure the build directory is properly set and that `BUILD_TESTING` is either unset or `OFF`.

**Required Components**:
- [Arm GNU Toolchain](https://gitlab.arm.com/tooling/gnu-toolchains-for-arm) (15.2.1 used in $v0.0.3$ release tests)
- [Arm UCX Libraries](https://github.com/openucx/ucx/releases) (HPCX v2.20/UCX v1.17.0)

The ARM cross-compilation system utilizes system paths for CMake module discovery.
This allows the use of cluster module files to build against versions of libraries shared over NFS.
To build AURORA using cross compilation:
1. First, purge all environment variables, ensuring UCX, or other build dependencies, are not in the system path
2. If needed, load CMake, and the cross compilation modules:
```sh
module load cmake
module load gcc-arm/15.2
```

3. Export the module path to contain only the paths to `aarch64` versions. See the below example for the HPC-AI Advisory Council ROME cluster:
```sh
export MODULEPATH="/global/software/rocky-9.$LOCAL_ARCH/modfiles/langs:/global/software/rocky-9.$LOCAL_ARCH/modfiles/tools:/global/software/rocky-9.$LOCAL_ARCH/modfiles/apps"
```

4. Load the build dependencies (now using the `aarch64` versions). Again, the example shown is for the HPC-AI Advisory Council ROME cluster:
```sh
module load gcc/11
module load hpcx/2.20 # UCX version 1.17
```

5. Build the code using the `aarch64-linux-gnu-toolchain`. Do *NOT*  build the testing suite for `aarch64`.
```sh
cmake \
	-DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu-toolchain.cmake \
	-Bbuild_aarch64 \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DBUILD_TESTING=OFF \ # ALWAYS OFF
	-DACR_MAX_WORKERS=16 \
	-DAIM_MAX_WORKERS=256 \
	-G "Unix Makefiles" 
```

For a full example of how to source things to setup the AArch64 vs x86_64 build system, see the `source scripts/clusters/rome/env.sh aarch64` script.
This script sets up the build environment needed to run `make build_bf` using the top level makefile.
# Build Directories Explained
- Each component (Server, Client, Common) of AURORA is build separately and linked
- The server is built under `./build/server` by default.
	- The generated executable is `aurora_remote_engine`
	- The executable can be run on any architecture (compiler dependent)
- The "client" component is build under `./build/lib`
	- The generated binary is `libaul.so`
	- Tests/programs using the lib need to link against this lib
	- Header files are not exported by default, but can be copied from `./lib/include`
- The common library (common to cli/srv)
	- The common lib is statically linked into both the Client and Server
	- Its output can be found under `./build/common`
	- Its output file is `libaurora_common.a`
# Running Tests
See [[Research/CheckpointRestart/AURORA/Testing/Testing|Testing]].