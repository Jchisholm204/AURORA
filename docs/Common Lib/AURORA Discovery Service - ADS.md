The AURORA Discovery Service module exists only to perform the discovery and connection logic between the client and server.
- The "server" component hosts the "Discovery Service" by calling `ads_init`
- Both the client and server can call `ads_request_exchange` to exchange a list of [[AURORA Blob]]s
- The exchange list is fixed within the `ads_exchange_data_t` type
- Requires:
	- [[AURORA Blob]]
	- MDNS (statically linked)
## Initialization
- The client side does **not** need to perform initialization.
- The server side can create a "connection listener" instance
- Only one instance can be active at a time (port restrictions)

```c
ads_hndl *ads_init(void);
```

## Deconstruction
- The client side does **not** need to perform any deconstruction actions
- The server side must finalize the ADS instance to properly close all ports

```c
int ads_finalize(ads_hndl **);
ads_hndl * pADS_hndl = ads_init(void);
// ...
int ads_status = 0;
ads_status = ads_finalize(&pADS_hndl);
if(ads_status != 0){
	// Error
}
```

## Configuration
Configuration is performed on a per call basis using the following structure:

```c
struct aurora_discovery_service_conf {
    char *opt_server_ip;
    int timeout_ms;
};
```

- `opt_server_ip`: Optional value specifying the IP of the server
	- Default: `NULL`
	- Uses MDNS to find the IP of the server (will use first found)
- `timeout_ms`: Exchange wait timeout (in ms)
## Standard Usage
Standard Usage/Sub-Components present within the ADS.
### Exchanging Data `request_exchange`
- Client side component for [[#Exchanging Data `exchange`]] and [[#Accepting Exchanges]]
- Requires:
	- A completed [[#Configuration]] struct
	- Local copy of `exchange_data` (from [[AURORA Connection Instance - ACI|ACI]] and [[AURORA Completion Notification - ACN|ACN]]) 
	- No additional initialization
- Can be called multiple times (does not maintain/use internal context)
- Usage:
	1. Allocate an `exchange_data` structure
	2. Initialize [[AURORA Connection Instance - ACI|ACI]] and [[AURORA Completion Notification - ACN|ACN]] with the `exchange_data` [[AURORA Blob|blobs]]
	3. Call on `ads_request_exchange`
	4. Connect [[AURORA Connection Instance - ACI|ACI]] and [[AURORA Completion Notification - ACN|ACN]] with the `exchange_data` [[AURORA Blob|blobs]] returned by the exchange function
	5. Free the pointer returned by this function
- Returns a completed `exchange_data` type, or `NULL` upon failure
```c
extern ads_exchange_data_t *ads_request_exchange(
    const ads_conf_t *pConf, ads_exchange_data_t *pTxData);
```

### Accepting Exchanges
- Blocking function to poll the ADS instance for a new connection
- Requires:
	- ADS to be initialized ([[#Initialization]])
- Can be called multiple times (uses the ADS instance context)
- Usage:
	- Run this as a singleton to connect to a single client
	- Run this in its own thread to wait for incoming client connections
- Returns a Socket File Descriptor or `0` on failure

```c
int ads_accept_any(ads_hndl *pHndl);
```
### Exchanging Data `exchange`
- Server side (part 2) component for [[#Exchanging Data `request_exchange`]]
- Can only be called once per socket descriptor
- Requires:
	- ADS to be initialized ([[#Initialization]])
	- A Socket File Descriptor returned from [[#Accepting Exchanges]]
- Returns a completed `exchange_data` type, or `NULL` upon failure

```c
extern ads_exchange_data_t *ads_exchange(int sock_fd,
                                         ads_exchange_data_t *pTxData);
```

## Notes
- All exchange data should follow the [[AURORA Blob]] format.
- Current implementation uses TCP sockets and an MDNS server to perform the exchange.
- This could/should be migrated to use the same key-store-value as MPI to initialize the system.