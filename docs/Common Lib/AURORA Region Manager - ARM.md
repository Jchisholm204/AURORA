# ARM
The AURORA Region Manager asynchronously manages a list of locally and remotely accessible memory segments between the client and server.
- Regions are automatically shared and registered on the remote end through UCP Active Messages.
- Segments can have custom `free(void*)` functions that will be called when the region is region is deallocated.
- All regions will be freed when the ARM instance is shut down.
- No internal local/remote sync system, everything is assumed to instantly execute through AM callbacks.
## Initialization
- Requires:
	- [[AURORA Connection Instance - ACI]] (Connected)
### Instance Creation
- Sets up the UCP active message handlers
- Will hold/use the ACI handle internally
- Requires:
	- [[AURORA Connection Instance - ACI#Instance Creation|ACI Instance Creation]]  to have successfully completed
- Returns: an ARM instance or NULL

```c
arm_hndl *arm_create_instance(aci_hndl *pACI);
```
## Deconstruction
- Can be called at any point to destroy an ARM instance
- Must be called before [[AURORA Connection Instance - ACI#Deconstruction]]
- Will invoke the `free(void*)` functions for all stored regions.
- Returns: An `eACN_error` type, `eACN_OK=0`

```c
eARM_error arm_destroy_instance(arm_hndl **ppHndl);
```
## Standard Interface
### Add Region
- Adds a region to the local "local" list and remotes "remote" list
- Parameters are not saved internally
- May reject a region that is not properly formatted
- Uses a "add" AM callback to complete the remote operation

```c
 /*
 * @brief Add/Setup a ARM Region
 *
 * @param pHndl ARM Handle to add to
 * @param pAMR Region (with user fields filled out)
 * @return
 */
eARM_error arm_add(arm_hndl *pHndl, const amr_hndl *pAMR);
```

### Remove Region
- Opposite function to [[#Add Region]]
- Will be executed instantly on the remote side (or when the worker is polled)
- Uses a "remove" AM callback to complete the remote operation
- Attempts to use "best guess" for region removal
	- Matching Name, ID, Pointer values, ..
	- Name can be left as Blank or `""` to ignore the name during the matching process
```c
/**
 * @brief Remove/Deinit an ARM Region
 *
 * @param pHndl ARM Handle to remove from
 * @param pAMR Match Criteria (Will remove any regions that match the values in
 * this struct)
 * This function may also be called internally free all local data structures
 * @return
 */
eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR);
```


### Get Region Counts
- Returns the local counts for the number of regions that exist
- Does not "attempt" to "synchronize" with the remote in any way. Assumes all pending operations have been completed
```c
/**
 * @brief Get the number of regions within the ARM
 *
 * @param pHndl ARM Handle
 * @return The number of regions associated with the handle
 */
size_t arm_get_n_remote_regions(arm_hndl *pHndl);
size_t arm_get_n_local_regions(arm_hndl *pHndl);
```

### Get Region Lists
- Returns a read only pointer to the internal ARM memory
- This is an unlocked list that may change during AM callbacks
- The local/remote should be externally synchronized to ensure all lists are synced before calling this function.
```c
/**
 * @brief Get read only access to a handles regions
 *
 * @param pHndl Handle to access
 * @return read only pointer to the internal ARM array
 */
const amr_hndl *arm_get_remote_regions(arm_hndl *pHndl);
const amr_hndl *arm_get_local_regions(arm_hndl *pHndl);
```

### Remote Write
- Wrapper around RDMA writes
- Unfolds to [[AURORA Connection Instance - ACI]] RDMA Write -> RDMA Write
```c
/**
 * @brief RDMA Write to a region (shadow or active)
 *
 * @param pHndl ARM Handle the region belongs to
 * @param pAMR Memory Region Handle
 * @param remote_addr Remote address to write to
 * @param data Local data to write
 * @param size size of the write
 * @return Err or OK
 */
eARM_error arm_write(arm_hndl *pHndl, const amr_hndl *pAMR,
                     const uint64_t remote_addr, const void *data,
                     size_t size);
```


### Remote Read
- Wrapper around RDMA reads
- Unfolds to [[AURORA Connection Instance - ACI]] RDMA Read -> RDMA Read
```c
/**
 * @brief RDMA Read from a region (shadow or active)
 *
 * @param pHndl ARM Handle the region belongs to
 * @param pAMR Memory Region Handle
 * @param remote_addr Remote address to read from
 * @param data region to copy the read data to
 * @param size size of the transfer
 * @return Err or OK
 */
eARM_error arm_read(arm_hndl *pHndl, const amr_hndl *pAMR,
                    const uint64_t remote_addr, void *data, 
                    size_t size);
```
