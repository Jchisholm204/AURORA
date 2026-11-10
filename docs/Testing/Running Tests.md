### Running Tests

After sourcing `scripts/env.sh` to set up all test variables,
[`scripts/run_tests.sh`](../../scripts/run_tests.sh) can be run to automatically
spawn the configured test jobs.

#### Selecting tests

Each test family is enabled with a top-level `true`/`false` selector in
`run_tests.sh` (`RUN_HEATDIS_AURORA`, `RUN_HEATDIS_VELOC`,
`RUN_HEATDIS_ORIGINAL`, `RUN_BLOCK_AURORA`).

#### `submit_test`

Jobs are submitted with:

```text
submit_test <script> <time> <name> <N_NODES> <test args...>
```

- `N_NODES` selects the first `N_NODES` entries of `AURORA_CLUSTER_NODES`.
- The existing test arguments (run count, memory, process count, backend
  workers) follow `N_NODES`.

#### Multi-node and multi-server

For `slurm` and `ssh`, all selected pairs are launched as **one** aggregated job:

- `TEST_NODES` is a comma-separated list of client nodes.
- `BACKEND_NODES` is a comma-separated list of backend nodes.
- One server is started per backend node (one `srun` task per node for `slurm`,
  one remote SSH process per node for `ssh`).
- The client is a single `mpirun` job across the test nodes.

For `host` and `none`, each configured pair still runs independently as its own
local job; `N_NODES` is ignored.

A single-node job keeps the name `<name>_<test_node>`. A multi-node job is named
`<name>_multi`.

> **Note**
> The test programs currently treat the backend argument as a single hostname.
> Multi-server launches therefore start every server, but a test only uses one
> until it is updated to split the backend list and map MPI ranks to servers
> (for example `servers[rank % n_servers]`). Use one pair until that mapping
> exists.
