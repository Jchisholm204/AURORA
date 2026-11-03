Before running the AURORA Remote Engine (ARE), ensure that it was correctly compiled for the target architecture.
Then, from the remote node, run:
```sh
aurora_remote_engine <opt_log_file_path>
```

**Parameters:**
1. Optional: Log File Path - Path to save the log output to, reduces logging in the terminal

## External Event Logging (AEL)
Set `AURORA_AEL_HOST` (and optionally `AURORA_AEL_PORT`, default `9064`) to stream connect/checkpoint/restart/disconnect events to an `aurora_external_logger` relay:
```sh
AURORA_AEL_HOST=<relay_ip> aurora_remote_engine <opt_log_file_path>
```
See [AURORA External Logger - AEL](../Common%20Lib/AURORA%20External%20Logger%20-%20AEL.md).

## Distributed Launches (SLURM/MPI)
Connection logic is handled on the client side, therefore there is no special procedure to perform a distributed launch of the application.
If launching with MPI, ensure that only one process is launched on each node, ie `-np` (number of ranks) == `-H` (number of nodes).
