# ACR
The AURORA Command runner is a server side component that handles thread dispatch for all client processing threads.
- Each thread is granted a thread context, containing the [[AURORA Instance Manager - AIM|AIM]] instance and an entry.
	- The instance allows the thread to return the entry to the instance upon task completion
	- The entry is the AIM client to perform the task on.
- [[#ACR]] threads are also given a "`flags`" parameter that can be used on a per command basis
- Each thread is given a memory scratchpad
	- A pre-allocated block of memory that the thread can use however it likes
	- The size is determined in `acr.h` and is defined as `ACR_CMD_CTX_SCRATCH_SIZE`
	- The size can be changed at compile time though the build system
- All threads share the same formatting.
- The ACR can run any thread with the standard format that accepts a command context
	- For simplicity, all threads that can be used with the ACR are currently defined within `acr.h`
	- Each thread is given its own `c` file, under the convention `acr_cmd_{name}.c`
	- Commands designed to run under the supervision of the ACR should not be run outside its context.

## Initialization
- Requires:
	- [[AURORA Instance Manager - AIM||AIM]] (Connected)
	- Number of concurrent workers (threads) to allow active at once
- This function pre-allocates nearly all of the memory needed for the ACR.
- It may have high overhead due to a number of large `malloc` invocations
- Returns:
	- A pointer to a ACR handle on success
	- NULL on failure

```c
acr_hndl *acr_init(aim_hndl *pAIM, size_t max_workers);
```
## Deconstruction
- Cleans up the ACR instance in whatever state it might be in
- This function may fail if threads are still active
- It can be called repeatedly to wait for all threads to complete
```c
eACR_error acr_finalize(acr_hndl **ppHndl);
```

## Standard Interface

### Dispatch Command Thread
- May be called by any thread at any time (thread safe)
- None of is arguments may be `NULL`
- Does **NOTHING** upon failure. The caller must determine what actions to take.
- Returns:
	- Instantly, either with success or `eACR_error`
	- Errors typically indicate that the max thread count has been reached.

```c
eACR_error acr_run(acr_hndl *pHndl, aim_entry_t *pInstance, int flags,
                   ACR_cmd_fn cmd_function);
```
### Command Thread Options
- Options for the `ACR_cmd_fn` argument within [[#Dispatch Command Thread]]
- All of these options should always be launched directly through the ACR
- Failure to launch these options through the ACR is undefined behavior
#### NOP
- NOPs the AIM entry
- Simply does nothing
- Does not even spawn a thread to handle the command.
- Simply returns the [[AURORA Instance Manager - AIM|AIM]] entry back to the queue
#### Checkpoint
- Performs a checkpoint operation on the instance
- Flags = 0 (ignored)
#### Restart
- Performs a restore operation on the instance
- Flags = 0 (ignored)
#### Connection Up
- Only be called to add a new connection to the server
- Precursor to [[#Connection Down]]
- Flags = socket opened by [[AURORA Connection Listener - ACL]]
- Instance = NULL (special case)
#### Connection Down
- Only be called to delete/clean up a connection
- Flags = 0 (ignored)
- Instance = the instance to remove
## Internal Interface
- To be used internally by the ACR and ACR commands
- Must define `ACR_INTERNAL` to access definitions from the ACR header

### Context Release
- Must be called by all threads/commands to release the pre-allocated context upon cleanup
- This function may fail if the server is busy. (Issue #27)
- Use [[#Context Release Retry]] for more consistent results
```c
eACR_error _acr_ctx_release(struct aurora_command_ctx *pCtx);
```
### Context Release Retry
- [[#Context Release]], but with an auto retry mechanism to ensure the release follows through
- `count` - The number of times to retry before giving up.
```c
eACR_error _acr_ctx_release_retry(struct aurora_command_ctx *pCtx, int count);
```