# AGENTS.md

AURORA (Accelerated User-space Remote Offload for Resilient Applications) is a
C11 checkpoint/restart framework. It builds with CMake and is split into a shared
`common` library, a client library, and a remote engine server.

## Reading the code (library -> subcomponent)

Each library directory contains subcomponents, one per 3-letter prefix. Read at
this depth first; descend into a subdirectory only when the task needs it.

| Library | Subcomponents |
|---|---|
| `common/` (static `libaroc.a`, shared by client and server) | `aci` UCX connection layer, `acn` completion notifications, `ads` mDNS discovery, `ael` external logger, `afv` checkpoint file format, `arm` region manager |
| `lib/` (client `libaul.so`, public headers in `lib/include/`) | `aul` user API, split by operation (`aul_init`, `aul_checkpoint`, `aul_restart`, `aul_mem`, `aul_configuration`) |
| `server/` (remote engine `aurora_remote_engine`) | `are` main loop, `aim` instance manager, `acr` command runner, `acl` connection listener |
| `tests/` | one directory per suite, each with its own `build.cmake` |

Also: `cmake/` (toolchain and `Find*.cmake`), `scripts/` (cluster/test drivers).

## Naming scheme

The prefix identifies the component and is used consistently everywhere:

- **Files:** `<prefix>_<aspect>.c` / `.h` — e.g. `afv_checkpoint_rw.c`, `arm_am.c`,
  `acr_cmd_checkpoint.c`. Headers live under `<library>/include/`, in a
  `<prefix>/` subdirectory when the component has several of them.
- **Functions:** `<prefix>_<verb>[_<object>]` — e.g. `arm_add`, `afv_write_metadata`,
  `aci_create_instance`. Internal/static helpers start with an underscore, e.g.
  `_arl_add`, `_aci_init_context`.
- **Public client API:** `AUL_*` (`AUL_Init`, `AUL_Checkpoint`, `AUL_Restart`).
- **Types:** handles are `<prefix>_hndl`, error enums are `e<PREFIX>_error`.
- **Docs:** one file per component, `AURORA <Full Name> - <PREFIX>.md`.

## Finding documentation

Start at `docs/AURORA.md`, then open the index for the area you are changing:

- `docs/Common Lib/Common Lib.md` — internal/common components
- `docs/Server Docs/Server Docs.md` — remote engine internals
- `docs/Client Lib/Client Lib.md` — public user API
- `docs/Testing/Testing.md` — test drivers and result reproduction

Each component doc is named after its prefix (`AURORA Region Manager - ARM.md`,
`AURORA File Versioning - AFV.md`, `AURORA Command Runner - ACR.md`, ...) and its
`##` sections map to that component's functions. To find behaviour, grep for the
prefix and read the matching doc:

```bash
grep -rn "arm_" common/include/arm common/src/arm
```

## Working in this environment

Read the agent docs before building or running anything:

- `docs/agent/building.md` — where the code is, how to compile it
- `docs/agent/sandbox.md` — sandbox provisioning and known limitations

## Guardrails

- Do not modify `results/`, `checkpoints/`, or `build/`; build into `/tmp/opencode`.
- Prefer `grep`/`glob` over reading whole files, and exclude the directories above
  from recursive scans.
