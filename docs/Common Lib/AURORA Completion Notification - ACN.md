# ACN
The AURORA Completion Notification module acts as a sync mechanism to keep track of completion progress.
- Both the client and server maintain a `completion_notification_memory` union/structure.
- These structures are shared between the client/server over RDMA such that both can access the structure of the other.
- RDMA Access should *only* be performed in a read only manner.
- The sync mechanism is performed using [[#Standard Interface|ticks]].
- Requires:
	- [[AURORA Connection Instance - ACI]]
	- [[AURORA Blob]]
	- UCP

## Initialization
- Uses the same initialization structure as the [[AURORA Connection Instance - ACI]]
- Requires:
	- [[AURORA Connection Instance - ACI]] (Instance, not connected)
	- Empty [[AURORA Blob]] from the [[ADS]]
### Instance Creation
- Allocates the internal data structures used for RDMA and the ACN instance.
- Will hold/use the ACI handle internally
- Requires:
	- [[AURORA Connection Instance - ACI#Instance Creation|ACI Instance Creation]]  to have successfully completed
	- An empty [[AURORA Blob]] structure
- Returns:
	- An unconnected ACN instance
	- Filled out exchange data ([[AURORA Blob]], return by parameter)

```c
acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info);
```
### Instance Connection
- Connects an ACN instance with the remote instance.
- This function also frees data from [[#Instance Creation]]
- Requires:
	- An existing ACN ([[#Instance Creation]])
	- The [[AURORA Discovery Service - ADS]] remote exchange to have successfully completed
- Returns: An `eACN_error` type, `eACN_OK=0`

```c
eACN_error acn_connect_instance(acn_hndl *pHndl,
                                aurora_blob_t *local_info,
                                aurora_blob_t *remote_info);
```
## Deconstruction
- Can be called at any point to destroy an ACN instance
- Returns: An `eACN_error` type, `eACN_OK=0`

```c
eACN_error acn_destroy_instance(acn_hndl **ppHndl);
```

## Standard Interface
- The standard interface describes system ticks
- Ticks keep track of the number of requests or operations triggered locally.
- To verify consistency between local and remote, the number of ticks is read from the remote over RDMA and compared to the local.
### Tick
- Can be used to tick one or multiple notification values one (1) operation forwards
- This function only accesses node-local memory
- Multiple notifications may be advanced with a single call
	- The `eACN_notification` enumeration is bit wise set
	- Several types of notifications can be combined using bit wise OR operations
- Requires:
	- A setup and functioning ACN Instance
	- A single or multiple notification values to tick forwards
- Returns: an `eACN_error`, `eACN_OK=0`

```c
eACN_error acn_tick(acn_hndl *pHndl, eACN_notification notifs);
```
### Await
- Can be used wait for the remote to synchronise with the local (Blocking Poll).
- This function accesses node-local and remote memory (RDMA)
- It will poll 
- Multiple notifications may be advanced with a single call
	- The `eACN_notification` enumeration is bit wise set
	- Several types of notifications can be combined using bit wise OR operations
- The implementation of the await function repeatedly calls the Ahead-Behind function until a zero is returned.
- This function will also poll on the UCP worker to ensure all pending UCP transactions are fulfilled.
- During this time, a  $\mu$ delay is used after every poll to ensure that the PCIe bus is not flooded.
- Requires:
	- A setup and functioning ACN Instance
	- A single or multiple notification values to wait on
- Returns: an `eACN_error`, `eACN_OK=0`

```c
eACN_error acn_await(acn_hndl *pHndl, eACN_notification notifs);
```
### Ahead-Behind
- Performs a remote read, followed by a compare operation.
- Calculates the difference between the number of operations performed on the remote vs the local.
- Negative numbers indicate that the local is ahead of the remote, while positive numbers indicate the remote is ahead of the local.
- The exact number, positive or negative, indicates how many operations are left to complete before the local and remote are synchronized.
- Requires:
	- A setup and functioning ACN Instance
	- A single or multiple notification values to sum
- Returns: Difference, or `0` on error. Do not use output for error handling

```c
int acn_aheadbehind(acn_hndl *pHndl, eACN_notification notifs);
```
### Check
- Can be used to check which notifications are out of sync.
- Requires:
	- A setup and functioning ACN Instance
	- Valid pointer to an `eACN_notifications` value
- Returns: 
	- `eACN_error`, `eACN_OK=0`
	- Returns by parameter `eACN_notification`, a bit set indicating which notifications are out of sync

```c
eACN_error acn_check(acn_hndl *pHndl, eACN_notification *pNotifs);
```
## Extended Interface
- Describes all interfaces not related to ticks, or not meant to be used in a typical sense
### Tick Set
- Can be used to set the local tick value. Not recommended.
- Only one notification value must be used.
- If multiple notifications are set, the lowest one will be set
- This function only accesses node-local memory.
```c
eACN_error acn_set(acn_hndl *pHndl, eACN_notification notif,
                   const uint64_t value);
```
### Tick Get
- Can be used to get the remote tick value. Not recommended.
- Only one notification value must be used.
- If multiple notifications are set, the lowest one will be set
- This function performs a blocking RDMA operation.

```c
eACN_error acn_get(acn_hndl *pHndl, eACN_notification notif,
                          uint64_t *pValue);
```
### Set Name
- Can be used set the ACN name variable
- This function only accesses node-local memory.

```c
eACN_error acn_set_name(acn_hndl *pHndl,
                               const char name[static ACN_NAME_LEN]);
```
### Get Name
- Can be used set the ACN name variable
- This function performs a blocking RDMA operation.

```c
eACN_error acn_get_name(acn_hndl *pHndl, char name[static ACN_NAME_LEN]);
```
