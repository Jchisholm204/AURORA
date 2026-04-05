The AURORA Connection Instance (ACI) encapsulates the UCP worker and endpoint structures.
- It exposes two interfaces; standard and internal.
- ACI is **NOT** thread safe.
- Each ACI instance must only be called from one thread at any given time.
- Requires:
	- [[AURORA Discovery Service - ADS]] (for key-value exchange)
	- UCP

## Initialization
The ACI must be initialized in two stages; [[#Instance Creation]] and [[#Instance Connection]].
### Instance Creation
- Create the UCP context and worker.
- This function will automatically create and manage a single, internal context used by all instances of the ACI.
	- Creates a UCP context if one does not already exist.
	- Creates a UCP worker attached to the context.
- Generates the worker connection ID, and places it within the `blob_t`
	- The memory returned by this function must not be freed
	- The memory placed within the `blob_t` must also not be freed
		- This memory must get passed to [[#ACI Connection|`aci_connect_instance`]]
		- The Connect Instance function will handle cleanup
- Requires:
	- An empty [[AURORA Blob]] structure
- Returns: 
	- Unconnected ACI instance
	- Filled out exchange data ([[AURORA Blob]], return by parameter)

```c
aci_hndl *aci_create_instance(aurora_blob_t *conn_info);
```

### Instance Connection
- Connects an ACI instance with the remote instance.
- Requires:
	- An existing ACI ([[#Instance Creation]])
	- The [[AURORA Discovery Service - ADS]] remote exchange to have successfully completed
- Returns: `0` or `ucs_status_t` (as int) on error

```c
extern int aci_connect_instance(aci_hndl *pHndl, aurora_blob_t *local_info,
                                aurora_blob_t *remote_info);
```
## Deconstruction
- Any other instances relying on an ACI should be destroyed before before the ACI itself.
- While ACI destruction is a two part process, [[#Instance Destruction]] will attempt to perform [[#Instance Disconnection]] before destroying the instance.
### Instance Disconnection
- Attempts to disconnect the instance from the remote
- Returns: `0` or error (nullparam is error)

```c
int aci_disconnect_instance(aci_hndl *pHndl);
```
### Instance Destruction
- Attempts to destroy everything associated with the connection
- Returns: `0` or error (nullparam is error)

```c
int aci_destroy_instance(aci_hndl **ppHndl);
```
## Standard Interface
- Designed to be a complete abstraction around **all** UCX/S/P datastructures.
- This layer should be the only/primary layer used outside of the [[AURORA Common Lib]]

### Polling
- This is a wrapper around `ucp_worker_progress`
- It can be progressed to handle incoming active message callbacks

```c
int aci_poll(aci_hndl *pHndl);
```
### Keep Alive
- Forces the internal UCP context to stay initialized even with 0 instances
- If called with `true` at some point, it must also be called with `false` before all ACI's are killed to ensure that the internal context is properly shut down.
- Useful if building a server application that may have 1 client rapidly connecting/disconnecting
- Setting this to `false` after closing all ACI's is not guaranteed to clean up any hanging contexts.

```c
void aci_keepalive(bool enable);
```

## Extended Interface
- Since this module abstracts the UCP worker and endpoint, any UCP functions that require access to these structures are abstracted through the [[#Extended Interface]]
- All functions in this interface follow the same template as the UCP functions they wrap, but with the endpoint/context replaced with an ACI.
- The UCP Active Message Send (`ucp_am_send_nbx`) wrapper is provided as an example.

```c
ucs_status_ptr_t aci_am_send_nbx(aci_hndl *pHndl, unsigned int id,
                                 const void *header, size_t header_len,
                                 const void *data, size_t data_len,
                                 const ucp_request_param_t *param);
```