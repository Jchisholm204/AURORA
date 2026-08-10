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
> The `ATH_BACKEND_PLATFOM` variable must be set to the right operating mode for the server to launch correctly.

#### `ATH_BACKEND_PLATFOM`
Determines the launch mechanism used for the server.
Possible options include:
```sh
export ATH_BACKEND_PLATFORM="none"
export ATH_BACKEND_PLATFORM="bf"
```

>!NOTE
> Only one ATH platform can be specified at a time.
> Use of `none` assumes that the backend will be run externally.
> IE, the testing environment will not launch the backend.
> Options are case-sensitive.


#### `AURORA_CLUSTER_CHECKPOINT_DIR`
Sets the directory to use for checkpoints.
This must be an absolute path, ending *without* a `/`.

#### `AURORA_CLUSTER_TMP_DIR`
Sets the directory to use temporary files, such as build artifacts.
This must be an absolute path, ending *without* a `/`.
