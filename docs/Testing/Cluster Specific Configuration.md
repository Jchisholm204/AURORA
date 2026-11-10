## Cluster Specific Configuration
Each cluster must contain its own testing environment setup scripts under `scripts/clusters/$CLUSTER_NAME/`.
Within this directory, two scripts control the environment setup (`env.sh`) and launch configuration (`launch_config.sh`).
The directory organization and script names must follow the outlined format.

### `env.sh`
The purpose of the cluster specific `env.sh` script is to load the modules required to compile and run AURORA:
- VeloC
- UCX
- MPI
- GCC
- CMake

Optionally, the `env.sh` script can load the ARM cross compilation toolchain.
If implemented, the script should use the first argument to determine the toolchain to load (`x86_64` or `aarch64`).

### `launch_config.sh`
The script should export the following variables, used in the generic testing scripts:

#### `AURORA_CLUSTER_NODES`
A ZSH style array consisting of node-BlueField hostname pairs.
Each pair is a comma delimited array consisting of the node and BF hostnames.
For example:

```sh
export AURORA_CLUSTER_NODES=(
    "host001,bf001"
    ...
    "host999,bf999"
)
```

Ordering is specific.
The names and format should match the names used in Slurm.
These names are used to launch the Slurm Job.
For cases where a BF is not used, the list can be modified as follows:
```sh
export AURORA_CLUSTER_NODES=(
    "host001"
    ...
    "host999"
)
```

>!NOTE
> `AURORA_LAUNCH_PLATFORM` must be set to the right operating mode for the
> server to launch correctly.

#### `AURORA_LAUNCH_PLATFORM`
Determines how the backend and client are launched. Possible options:
```sh
export AURORA_LAUNCH_PLATFORM="host"   # local server + local mpirun
export AURORA_LAUNCH_PLATFORM="slurm"  # server via srun on the backend node
export AURORA_LAUNCH_PLATFORM="ssh"    # server over SSH on the backend node
export AURORA_LAUNCH_PLATFORM="none"   # external backend, client only
```

>!NOTE
> Only one launch platform can be specified at a time.
> `slurm` and `ssh` require `"client,backend"` node pairs.
> `host` uses single-node entries.
> Use of `none` assumes that the backend will be run externally; the testing
> environment will not launch the backend.
> Options are case-sensitive.

#### `AURORA_SSH_USER`, `AURORA_SSH_OPTS`, `AURORA_REMOTE_ENV` (optional)
Overrides for `ssh` launches. SSH is assumed to be userless and passwordless by
default; set `AURORA_SSH_USER` only when a username is required.


#### `AURORA_CLUSTER_CHECKPOINT_DIR`
Sets the directory to use for checkpoints.
This must be an absolute path, ending *without* a `/`.

#### `AURORA_CLUSTER_TMP_DIR`
Sets the directory to use temporary files, such as build artifacts.
This must be an absolute path, ending *without* a `/`.
