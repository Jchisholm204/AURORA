# Agent Sandbox Environment

Notes for building AURORA inside the containerized agent sandbox. Written from a
verified end-to-end run (build exit 0, 10 test binaries produced).

## Container characteristics

| Property | Value |
|---|---|
| Base OS | Debian GNU/Linux 12 (bookworm) |
| Kernel | 7.2.11-200.fc44.x86_64 (host kernel, `odp0k`) |
| User | `coder` (uid 1000), with `sudo` = `(ALL) NOPASSWD: ALL` |
| Seccomp | enabled, `CapEff: 0` (privilege comes from `sudo`, not caps) |
| Network | outbound HTTPS + DNS available |
| RDMA hardware | none (UCX runs without the async-progress path) |

## Important: the sandbox is ephemeral

The container is reset on restart:

- **Lost on restart:** everything in `/tmp` (including `/tmp/opencode`), everything
  installed into `/usr/local` (including a from-source UCX), and all `apt` packages.
- **Survives:** the repository at `/Documents/AURORA`, because it is a bind mount
  from the host (and is mutagen-synced).

Consequence: provisioning must be repeated each session. If this becomes routine,
bake the steps below into the image instead.

## Repository ownership

The checkout is owned by `root:root`. It was made writable for all users with:

```bash
sudo chmod -R a+rwX /Documents/AURORA
```

Ownership was deliberately **not** changed. Git still reports `dubious ownership`
for `coder`, so either pass `-c safe.directory=/Documents/AURORA` or run:

```bash
git config --global --add safe.directory /Documents/AURORA
```

## Step 1 — Toolchain and dependencies

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    gcc g++ make libc6-dev cmake pkg-config \
    openmpi-bin libopenmpi-dev \
    libibverbs-dev librdmacm-dev libnuma-dev
```

Notes:

- `libc6-dev` must be listed explicitly. With `--no-install-recommends`, `gcc`
  alone does **not** pull it in, and every compile fails on `#include <sys/socket.h>`.
- `g++` is required even though AURORA is pure C (0 `.cpp` files). `project()` in
  `CMakeLists.txt` enables both C and CXX, and `cmake/host-linux-gnu-toolchain.cmake`
  hardcodes `set(CMAKE_CXX_COMPILER g++)`. A command-line `-DCMAKE_CXX_COMPILER`
  cannot override it.
- `openmpi-bin` pulls in `libucx0` (1.13.1) as a hard dependency. That package is
  left installed — see the UCX section for why removing it is a bad idea.

Versions resolved: GCC 12.2.0, CMake 3.25.1, OpenMPI 4.1.4.

## Step 2 — UCX 1.17.0 from source

Debian bookworm ships UCX **1.13.1**, which is too old. AURORA uses
`UCP_AM_SEND_FLAG_COPY_HEADER` (`common/src/arm/arm_rm.c`), which does not exist in
1.13.1, and `docs/Building.md` targets UCX 1.17.0 / HPCX 2.20. The build fails with:

```
error: 'UCP_AM_SEND_FLAG_COPY_HEADER' undeclared
```

Install 1.17.0 into `/usr/local`:

```bash
cd /tmp/opencode
curl -sSLO https://github.com/openucx/ucx/releases/download/v1.17.0/ucx-1.17.0.tar.gz
tar xzf ucx-1.17.0.tar.gz
cd ucx-1.17.0
./configure --prefix=/usr/local \
    --enable-mt --with-verbs --with-rdmacm --with-numa --enable-devel-headers
make -j"$(nproc)"
sudo make install
sudo ldconfig
```

Verify:

```bash
/usr/local/bin/ucx_info -v | head -1     # Library version: 1.17.0
ldd /usr/local/bin/ucx_info | grep libucp  # => /usr/local/lib/libucp.so.0
```

### Why `libucx0` (1.13.1) stays installed

`libopenmpi3` has a hard `Depends: libucx0`, so purging it also removes
`openmpi-bin`, `libopenmpi3`, and `libopenmpi-dev`. Five test targets link
`AURORA::mpi`, so without OpenMPI the CMake generate step fails before compiling
anything. Keeping 1.13.1 is harmless: `/usr/local/lib` is listed first in
`/etc/ld.so.conf.d/libc.conf`, so `ldconfig` resolves `libucp.so.0` to the 1.17.0
copy and the old one is never loaded.

Do **not** install `libucx-dev`, and purge it if present — it puts 1.13.1 headers
in `/usr/include/ucp`, which shadow-conflicts with the `/usr/local` install:

```bash
sudo apt-get purge -y libucx-dev ucx-utils
```

## Step 3 — Build

Run from the repository root (the Makefile uses the relative toolchain path
`cmake/host-linux-gnu-toolchain.cmake`, and CMake takes the source dir from the cwd):

```bash
UCX_HOME=/usr/local make BUILD_DIR=/tmp/opencode/aurora-build clean all
```

- `BUILD_DIR` is a plain `:=` assignment in the `Makefile`, so a command-line
  override redirects all output. `clean` then becomes
  `rm -rf /tmp/opencode/aurora-build`, which never touches the repo.
- `UCX_HOME=/usr/local` is consumed by `cmake/host-linux-gnu-toolchain.cmake`,
  which appends it to `CMAKE_PREFIX_PATH` so `find_package(ucx)` resolves to
  `/usr/local/lib/cmake/ucx`.
- This follows the default `gen_cmake_host` target, which hardcodes
  `-DBUILD_TESTING=ON`, so the test suite is compiled too.

Expected configure output:

```
-- Found UCX though CMake
-- Found MPI with CMAKE
CMake Warning at cmake/FindVeLOC.cmake:16 (message):
  Failed to find VELOC
```

The VeLOC warning is expected and benign: VeLOC is not installed, so the VeLOC
tests are skipped automatically.

### Artifacts

```
/tmp/opencode/aurora-build/common/libaroc.a
/tmp/opencode/aurora-build/lib/libaul.so
/tmp/opencode/aurora-build/server/aurora_remote_engine
```

Ten test binaries (VeLOC-free): `test_ael_server`, `test_ael_client`,
`test_afv_checkpoint`, `test_afv_metadata`, `test_afv_restore`, `test_discovery`,
`test_discovery_mpi`, `heatdis_aurora`, `heatdis_original`, `block_test_aurora`.

## Pitfalls

### Do not build into the repo's `build/` directory

`build/` is mutagen-synced from the developer's host machine. Its
`CMakeCache.txt` contains `CMAKE_HOME_DIRECTORY=/home/jacob/Documents/AURORA` and
Fedora MPI paths (`/usr/lib64/openmpi/bin/mpicc`). CMake refuses to reuse a cache
built for a different source dir, so keeping it out of the build path avoids a
confusing failure. Always override `BUILD_DIR`.

### The multiarch trap when UCX is distro-packaged

`cmake/host-linux-gnu-toolchain.cmake` sets `CMAKE_SYSTEM_NAME`, which puts CMake
into cross-compiling mode. Cross mode suppresses multiarch library search, so
`/usr/lib/x86_64-linux-gnu` is never scanned and `find_library()` fails for
`libucp` and `libz` alike — while `find_path()` for headers still succeeds. This is
why `FindUCX.cmake` reports `Failed to find UCX` on a stock Debian install.

The `/usr/local` UCX install sidesteps this entirely (`/usr/local` *is* searched in
cross mode, and its CMake config short-circuits the fallback). If you must use
distro-packaged UCX, add:

```
-DCMAKE_LIBRARY_ARCHITECTURE=x86_64-linux-gnu
```

### `mpirun` does not work in this sandbox

`mpirun` hangs (the TCP BTL has no usable interface). Builds are unaffected, but
the five MPI-based tests will not run here. Note also that
`aurora_remote_engine` treats `argv[1]` as a **log file path**, not a flag —
running `aurora_remote_engine --help` creates a file literally named `--help`.

### No RDMA hardware

UCX will build and run without it, but AURORA's asynchronous progress requires
UCX-compatible RDMA hardware and will not engage here.

## Minimal re-provisioning script

```bash
set -e
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    gcc g++ make libc6-dev cmake pkg-config \
    openmpi-bin libopenmpi-dev \
    libibverbs-dev librdmacm-dev libnuma-dev

cd /tmp/opencode
curl -sSLO https://github.com/openucx/ucx/releases/download/v1.17.0/ucx-1.17.0.tar.gz
tar xzf ucx-1.17.0.tar.gz && cd ucx-1.17.0
./configure --prefix=/usr/local --enable-mt \
    --with-verbs --with-rdmacm --with-numa --enable-devel-headers
make -j"$(nproc)" && sudo make install && sudo ldconfig

cd /Documents/AURORA
UCX_HOME=/usr/local make BUILD_DIR=/tmp/opencode/aurora-build clean all
```
