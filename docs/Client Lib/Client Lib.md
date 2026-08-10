The Client Library is used by applications to perform checkpoint-restore operations in connection with the remote engine (server).
The client library binds were chosen to mimic that of [VeloC](https://github.com/ECP-VeloC/VELOC/tree/main).
Therefore, this project is, in theory, "compatible" with any codebase currently using the VeloC Checkpoint Restore mechanism.

## Initialization
- AURORA is initialized using the [[AURORA Client Configuration]] structure
- Initialization of AURORA should take place after MPI initialization
	- The MPI initialization is independent of AURORA
	- AURORA may be initialized without MPI
- The return status of the init function should be checked to ensure a connection to the server has been established.
```c
int AUL_Init(const aul_configuration_t *pCFG);
```

## Memory Registration
- Used to add and remove memory regions to/from the check pointing framework
- Region IDs must be unique to each memory region, and persistent across application restarts
- **Warning**: Memory registration calls perform RDMA memory pinning operations (high performance overhead).
### Registering a Region
- Adds a memory region to the AURORA framework to be saved during checkpoint operations
- Requires:
	- AURORA to have initialized successfully
	- A unique memory ID, persistent across application restarts
	- Pointer to the memory starting address
	- Length of saved memory block
- Returns:
	- `0` if OK
	- Positive value for [[AURORA Completion Notification - ACN]] errors
	- Negative value for [[AURORA Region Manager - ARM]] errors

```c
int AUL_Mem_protect(const uint64_t mem_id, const void *const ptr,
                    const size_t size);
```
### Un-registering a Region
- Removes a memory region from the AURORA framework.
- Removed regions will not be saved in future checkpoints.
- Requires:
	- AURORA to have initialized successfully
	- The Unique ID of the added region
- Returns:
	- `0` if OK
	- Positive value for [[AURORA Completion Notification - ACN]] errors
	- Negative value for [[AURORA Region Manager - ARM]] errors

```c
int AUL_Mem_unprotect(const uint64_t mem_id);
```

### Checkpoint
- Performs a checkpoint of the regions currently added to AURORA
- Requires:
	- AURORA to have initialized successfully
	- The version and name of the saved checkpoint
		- `version` must be a positive number
		- `name` must not exceed `AUL_NAME_LEN` characters
- Returns:
	- `0` if OK

```c
int AUL_Checkpoint(const int version,
                   const char name[static AUL_NAME_LEN]);
```
### Test Restore
- Tests if a checkpoint can be restored.
	1. The file can be loaded from disk
	2. The loaded file matches the requested metadata
	3. The application has registered all regions required for the restore
- Requires:
	- AURORA to have initialized successfully
	- The version and name of the checkpoint to test 
		- `-1` and/or `NULL` to load the latest available checkpoint version
		- `version` must be a positive number
		- `name` must not exceed `AUL_NAME_LEN` characters
- Returns:
	- The version of the checkpoint found/requested if OK
	- A Negative value on error

```c
int AUL_Test(const int version, const char *name);
```
### Restore
- Performs a restore operation if:
	1. The file can be loaded from disk
	2. The loaded file matches the requested metadata
	3. The application has registered all regions required for the restore
- Requires:
	- AURORA to have initialized successfully
	- The version and name of the checkpoint to restore
		- `version` must be a positive number
		- `name` must not exceed `AUL_NAME_LEN` characters
- Returns:
	- The version of the checkpoint restored if OK
	- A Negative value on error

```c
int AUL_Restart(const int version, const char name[static AUL_NAME_LEN]);
```