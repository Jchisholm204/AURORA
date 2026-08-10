Before running the AURORA Remote Engine (ARE), ensure that it was correctly compiled for the target architecture.
Then, from the remote node, run:
```sh
aurora_remote_engine <opt_log_file_path>
```

**Parameters:**
1. Optional: Log File Path - Path to save the log output to, reduces logging in the terminal

## Distributed Launches (SLURM/MPI)
Connection logic is handled on the client side, therefore there is no special procedure to perform a distributed launch of the application.
If launching with MPI, ensure that only one process is launched on each node, ie `-np` (number of ranks) == `-H` (number of nodes).
