This document details the build instructions for AURORA.

# Prerequisites
## System Dependencies
- CMake ($\ge$ v3.21)
- UCX
	- UCT
	- UCS
	- UCP
- Pthreads

## Code Dependencies
All code dependencies are statically linked and should be pulled in using Git Submodules.
- Log Log (pulled from loglog repo, in common lib)
- MDNS (pulled from openmdns repo, in common lib ACL)

## Hardware Dependencies
This code has been optimized for NVIDIA BlueField-2/3 DPUS.
However, it is not tied to any specific architecture.
Both the client and server components are hardware agnostic with the exception of DMA hardware.

While some verification can be done without RDMA capable hardware, asynchronous progress will NOT take place unless both the client and server have UCX compatible RDMA hardware.

# Building
1. Install dependencies
2. Build the code
3. Examine the resulting executables

## Building the Code
The following CMake command can be used to generate the build system for all AURORA components.
It is also provided within the top level Makefile.

```sh
cmake \
	-DCMAKE_C_COMPILER=gcc\
	-DCMAKE_CXX_COMPILER=g++\
	-B${BUILD_DIR} \
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-G "Unix Makefiles"
```

With the dependencies installed, the above command can be executed with:
```sh
> make build all
```

> **NOTE:**
> No install mechanism is present in the top level Makefile.
> Install information can be found under the releases page.
> Both tarball releases and compiled RPMs are available for RHEL 10 compatible systems.

## BlueField Cross Compilation
*Not yet documented.*

## Build Directories Explained
- Each component (Server, Client, Common) of AURORA is build separately and linked
- The server is built under `./build/server` by default.
	- The generated executable is `aurora_remote_engine`
	- The executable can be run on any architecture (compiler dependent)
- The "client" component is build under `./build/lib`
	- The generated binary is `libaurora_user.so`
	- Tests/programs using the lib need to link against this lib
	- Header files are not exported by default, but can be copied from `./lib/include`
- The common library (common to cli/srv)
	- The common lib is statically linked into both the Client and Server
	- Its output can be found under `./build/common`
	- Its output file is `libaurora_common.a`

## Running Tests
*Not yet documented*