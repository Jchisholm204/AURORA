## Generic Testing Platform
[`scripts/env.sh`](../../scripts/env.sh) sets all global testing variables.
At the top of the file:

#### `AURORA_CLUSTER_NAME`
The cluster name, or the name of the cluster folder.
This should not be a full path, and must match one of the folders within `scripts/clusters`.

#### `AURORA_LOG_DIR`
Sets the output directory for all log files.
It is recomended to use the default `results/test_version` layout.
Results will then be saved to the `AURORA/results` directory.
