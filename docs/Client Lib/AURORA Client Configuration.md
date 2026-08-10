# AURORA Client Configuration
The Client Configuration structure is used by applications to initialize the AURORA framework.
The full structure is shown below:
```c
typedef struct {
    // MPI
    uint64_t rank;
    uint64_t opt_group_id;
    int64_t opt_group_size;

    // Checkpointing
    char *persistent_path;
    bool use_error_correction;

    // Connection
    enum eAULConnMode connection_mode;
    char *opt_ip;
    char *opt_hostname;

    // Logging
    char *opt_log_file;
} aul_configuration_t;
```

To automatically apply default settings, initialize the configuration structure with `AUL_CONFIG_DEFAULT`, included with `aul/aul.h`:

```c
const aul_configuration_t AUL_CONFIG_DEFAULT = {
    .rank = 0,
    .opt_group_id = 0,
    .opt_group_size = -1,
    .persistent_path = "./checkpoints",
    .use_error_correction = false,
    .connection_mode = eAULCModeAuto,
    .opt_ip = NULL,
    .opt_hostname = NULL,
    .opt_log_file = NULL,
};
```
## Configuration Options
### Rank
- Non optional, set to 0 to if not using multiple ranks
- Set to the MPI rank ID if using MPI
### Group ID
- Optional, Defaults to 0
- Can be set to a unique identifier if running multiple independent jobs with the same back-end server.
### Group Size
- Optional, set to `-1` to disable
- Can be set to the total number of MPI ranks in the group to enable failure detection
### Persistent Checkpoint Path
- Non optional, this parameter must be set to a valid system directory with `drw-` permissions.
- Set `persistent_path` to the path where the persistent checkpoints should be stored
- A temporary checkpoint path is not needed as temporary checkpoints are stored in RAM
### Connection Mode
- Sets the mode to use when attempting connection to the server.
- Possible Connection Modes:
    - `eAULCModeAuto` - AURORA will attempt all connection methods, first to succeed wins
    - `eAULCModeHostName` - Connects to `opt_hostname` (hostname of the server)
    - `eAULCModeLocalHost` - Connects to `127.0.0.1` (localhost)
    - `eAULCModeTargetIP` - Connects to the server running at `opt_ip`
### Backend Server IP
- Optional, set to the IP of the machine [Running ARE](Running%20ARE).
### Backend Server Hostname
- Optional, set to the hostname of the machine [Running ARE](Running%20ARE).
### Log File Output
- Optional, set to `NULL` to ignore
- Directory to save the rank's log file
- Directs all AURORA logs into the file
