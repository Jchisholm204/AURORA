# ACL
The AURORA Connection Listener module is a server specific connection manager that manages the threading aspect of the [AURORA Discovery Service - ADS](../Common%20Lib/AURORA%20Discovery%20Service%20-%20ADS.md).
- Init function spawns its on management thread.
- Should only be initialized according to [AURORA Discovery Service - ADS](../Common%20Lib/AURORA%20Discovery%20Service%20-%20ADS.md) specifications.
- Internally holds [AURORA Discovery Service - ADS](../Common%20Lib/AURORA%20Discovery%20Service%20-%20ADS.md) and [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM.md)
- Requires:
	- [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM.md) (Initialized Instance)
	- [AURORA Discovery Service - ADS](../Common%20Lib/AURORA%20Discovery%20Service%20-%20ADS.md)

## Initialization
- Allocates threading data structures (used internally only)
- Allocates an internal [AURORA Discovery Service - ADS](../Common%20Lib/AURORA%20Discovery%20Service%20-%20ADS.md)
- Holds and uses the [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM.md) internally
- Spawns the "[#Connection Listener Thread](#Connection%20Listener%20Thread)" 
- Will spawn and cleanup several threads during operation (does not use pool)
- Requires:
	- [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM.md) to have been initialized
- Returns:
	- Pointer to an ACL instance or NULL on error

```c
acl_hndl *acl_init(aim_hndl *pAIM);
```
## Deconstruction
- Cancels the main "connection listener" thread
- Waits for the thread to join the main thread
- Requires:
	- An existing ACL ([#Initialization](#Initialization))
- Returns 0 or null parameter

## Internal Threads
### Connection Listener Thread
- Spawned by ACL [#Initialization](#Initialization)
- Runs until ACL [#Deconstruction](#Deconstruction)
- Blocks on [ADS - Accepting Exchanges](../Common%20Lib/AURORA%20Discovery%20Service%20-%20ADS.md#Accepting%20Exchanges)
- Reports failures through the logger
- Spawns a [#Connection Acceptance Thread](#Connection%20Acceptance%20Thread) when the ADS returns a new connection

### Connection Acceptance Thread
- Creates all server side instances and adds them to the [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM.md)
	- [AURORA Connection Instance - ACI](../Common%20Lib/AURORA%20Connection%20Instance%20-%20ACI.md)
	- [AURORA Region Manager - ARM](../Common%20Lib/AURORA%20Region%20Manager%20-%20ARM.md)
	- [AURORA Completion Notification - ACN](../Common%20Lib/AURORA%20Completion%20Notification%20-%20ACN.md)
- Spawned by [#Connection Listener Thread](#Connection%20Listener%20Thread)
	- CHANGED April 2, 2026
	- Now spawned using the [AURORA Command Runner - ACR](AURORA%20Command%20Runner%20-%20ACR.md)
	- Handle is left NULL, socket is placed in the "flags" parameter
	- Can time out if the ACR does not have enough slots/server is busy
	- Changed in PR `refactor-acr`
- Runs until the connection process fails or completes.
- Logs all output/errors to the logger