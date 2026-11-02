# Building Inside the Agent Sandbox

Agent-facing guide: where the code is, and how to compile it **outside** the
AURORA working tree. Provisioning (compiler, UCX, MPI) is covered in
[`sandbox.md`](./sandbox.md) — read that first if `cmake` or `ucx_info` are missing.

## Finding the code

The checkout is bind-mounted at `/Documents/AURORA`. It is **mutagen-synced** with
the developer's machine, so it changes under you: `HEAD` moves and files appear
mid-session. Re-check state before trusting anything you read earlier.

```bash
cd /Documents/AURORA
git -c safe.directory=/Documents/AURORA log --oneline -1
git -c safe.directory=/Documents/AURORA status --short
```

The tree is owned by `root:root` but is world-writable (`chmod -R a+rwX`).
Git reports `dubious ownership`, hence the `-c safe.directory=...` above.

### Layout

| Path | Contents |
|---|---|
| `CMakeLists.txt`, `Makefile` | Top-level build entry points |
| `cmake/` | Toolchain files, `FindUCX.cmake`, `FindMPI.cmake`, `FindThreads.cmake`, `FindVeLOC.cmake` |
| `common/` | Static library (`libaroc.a`): protocol + checkpoint format |
| `common/src/aci/` | Low-level UCX connection layer (`aci*`) |
| `common/src/acn/` | Connection notifications (`acn*`) |
| `common/src/ads/` | Discovery service (mDNS) |
| `common/src/ael/` | AURORA External Logger (`ael`) — network log client/server |
| `common/src/afv/` | AURORA file format / checkpoint records |
| `common/src/arm/` | AURORA Region Manager (`arm`) — checkpoint regions, UCX AM messaging |
| `lib/` | Shared client library (`libaul.so`), public headers in `lib/include/` |
| `server/src/` | Remote engine (`aurora_remote_engine`): `aim` dispatch, `acr`, `acl` listener, `are` main loop |
| `tests/` | Test programs, one directory per suite, each with a `build.cmake` |
| `docs/` | Design docs, server/client API docs, build instructions |
| `scripts/` | Cluster env scripts, test drivers |
| `checkpoints/`, `results/` | Data only — **`results/` is ~67 GB**, never copy the repo wholesale |

### Searching

Prefer `grep`/`glob` over reading whole files; the tree is small (~3.2 MB of
source) but `results/` is enormous and must be excluded:

```bash
grep -rn "UCP_AM_SEND_FLAG" --include=*.c --include=*.h common server lib
glob "common/src/**/*.c"
```

Ignore `results/`, `checkpoints/`, `build/`, `.cache/` in any recursive scan.

### Primary entry points

- Server main: `server/src/main.c` → `are_main()` in `server/src/are.c`
- Client API: `lib/include/aul.h`, defaults in `lib/src/aul_configuration.c`
- Build wiring: root `CMakeLists.txt`, then `common/`, `server/`, `lib/`, `tests/`

## Build in your own directory, never in `build/`

`/Documents/AURORA/build` is **synced from the developer's host machine** and
holds a `CMakeCache.txt` with `CMAKE_HOME_DIRECTORY=/home/jacob/Documents/AURORA`
plus Fedora MPI paths. CMake will refuse to reuse it, and writing into the synced
tree can push agent artifacts back to the developer's machine.

Build into `/tmp/opencode/aurora-build` instead. `BUILD_DIR` is a plain `:=`
assignment in the `Makefile`, so a command-line override relocates **all** output.
`clean` becomes `rm -rf /tmp/opencode/aurora-build`, so it can never delete the
repo's own `build/` or any source.

```bash
cd /Documents/AURORA        # required: toolchain path in the Makefile is relative
UCX_HOME=/usr/local make BUILD_DIR=/tmp/opencode/aurora-build clean all
```

- `UCX_HOME=/usr/local` is read by `cmake/host-linux-gnu-toolchain.cmake`, which
  appends it to `CMAKE_PREFIX_PATH`; required because distro UCX (1.13.1) is too
  old for the AM send flags AURORA uses.
- The default `gen_cmake_host` target hardcodes `-DBUILD_TESTING=ON`, so tests are
  always compiled by `make`.
- Add `BUILD_TYPE=Release` for optimised builds (`BUILD_TYPE ?= Debug` by default).

### Output locations

```
/tmp/opencode/aurora-build/common/libaroc.a
/tmp/opencode/aurora-build/lib/libaul.so
/tmp/opencode/aurora-build/server/aurora_remote_engine
/tmp/opencode/aurora-build/tests/<test_name>
/tmp/opencode/aurora-build/compile_commands.json
```

`compile_commands.json` is generated there — point clangd at it rather than
expecting one in the source tree.

### Rebuild / reset

```bash
# incremental (keep objects)
UCX_HOME=/usr/local make BUILD_DIR=/tmp/opencode/aurora-build build

# from scratch (wipes ONLY the temp dir)
UCX_HOME=/usr/local make BUILD_DIR=/tmp/opencode/aurora-build clean all
```

If the repo moves to a new commit between builds, prefer a clean rebuild —
mutagen can rename or add files, and stale objects mask breakage.

### Base package only (no tests)

The Makefile offers no way to pass extra `-D` flags, and `-DBUILD_TESTING=ON` is
hardcoded. To skip the tests, bypass `gen_cmake_host` and configure directly:

```bash
cd /Documents/AURORA
UCX_HOME=/usr/local cmake \
    -DCMAKE_TOOLCHAIN_FILE=cmake/host-linux-gnu-toolchain.cmake \
    -B/tmp/opencode/aurora-build-min \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_TESTING=OFF \
    -G "Unix Makefiles"
make -C /tmp/opencode/aurora-build-min -j"$(nproc)"
```

Use this only when tests are irrelevant — e.g. after removing OpenMPI, since five
test targets link `AURORA::mpi` and the generate step fails without it.

## Verifying a build

```bash
# configure actually found the right deps
grep -E "UCX_FOUND|MPI_C_COMPILER:" /tmp/opencode/aurora-build/CMakeCache.txt

# right UCX version at runtime?
ldd /tmp/opencode/aurora-build/server/aurora_remote_engine | grep libuc
# expect: /usr/local/lib/libucp.so.0   (1.17.0)

# tests present? VeLOC ones must be absent when VeLOC is not installed
find /tmp/opencode/aurora-build/tests -maxdepth 1 -type f -perm -u+x
```

A healthy configure prints `Found UCX though CMake` and `Found MPI with CMAKE`,
plus a benign `Failed to find VELOC` warning.

## Running things (limited)

- `aurora_remote_engine` must be running for client tests to connect. Its
  `argv[1]` is a **log file path**, not a flag — `--help` creates a file named
  `--help` in the cwd. Start it as:
  `./aurora_remote_engine /tmp/opencode/server.log &`
- `mpirun` hangs in this sandbox (no usable TCP interface), so the MPI tests
  (`test_discovery_mpi`, `block_test_aurora`, `heatdis_*`) will not run here.
- No RDMA hardware: UCX works, AURORA's async-progress path does not engage.

## Etiquette

- Do not modify `build/`, `results/`, or `checkpoints/`.
- Check `git status` first; the developer frequently has uncommitted work in the
  tree. Never revert or reformat files you did not create.
- Keep all generated artifacts under `/tmp/opencode/`.
- The sandbox is ephemeral: `/tmp` and `/usr/local` are wiped on restart, so
  rebuild from the script in [`sandbox.md`](./sandbox.md) when tools go missing.
