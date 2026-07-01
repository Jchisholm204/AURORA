# Testing Scripts
The Testing Scripts encompass:
1. The high-level testing framework that submits tests and gathers results
2. Cluster-specific environment setups and launch variables
3. Generic testing platforms

All global variables used in testing scripts are prefixed with `ATH` (AURORA Testing Harness).

## High-Level Testing Framework

## Cluster Specific Variables
Each cluster must contain its own testing environment setup script.
This script should be under `scripts/$CLUSTER_NAME/launch_configuration.sh`.
The script should export the following variables, used in the generic testing scripts:


#### `ATH_NODES`
A ZSH style array consisting of node-BlueField hostname pairs.
Each pair is a comma delimited array consisting of the node and BF hostnames.
For example:

```sh
export ATH_NODES=(
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
export ATH_NODES=(
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
# export ATH_BACKEND_PLATFORM="none"
export ATH_BACKEND_PLATFORM="BF"
```

>!NOTE
> Only one ATH platform can be specified at a time.
> Use of `none` assumes that the backend will be run externally.
> Options are case-sensitive.


## Generic Testing Platform
Located under [`scripts/tests`](../../scripts/tests), each of the following tests have their own "generic" test launch script:

- Test: Launches the most basic 'does it crash or not' test
- Blocking Test: Used to generate all figures from `0.0.0`. Each MPI rank checkpoints/restores $\frac{\text{mem}}{n}$ KB.
- Heat Distribution Benchmark: A modified version of the heat distribution benchmark used to characterize VELOC.

### Test Script Parameters
Each of these scripts uses a number of parameters to determine their launch parameters at launch time.
Parameters are set via the cluster specific testing platform `launch_configuration.sh` script and the high level testing framework.

