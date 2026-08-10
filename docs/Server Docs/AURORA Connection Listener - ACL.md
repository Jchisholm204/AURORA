# ACL
The AURORA Connection Listener module is a server specific connection manager that manages the threading aspect of the [AURORA Discovery Service - ADS](AURORA%20Discovery%20Service%20-%20ADS).
- Init function spawns its on management thread.
- Should only be initialized according to [AURORA Discovery Service - ADS](AURORA%20Discovery%20Service%20-%20ADS) specifications.
- Internally holds [AURORA Discovery Service - ADS](AURORA%20Discovery%20Service%20-%20ADS) and [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM)
- Requires:
	- [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM) (Initialized Instance)
	- [AURORA Discovery Service - ADS](AURORA%20Discovery%20Service%20-%20ADS)

## Initialization
- Allocates threading data structures (used internally only)
- Allocates an internal [AURORA Discovery Service - ADS](AURORA%20Discovery%20Service%20-%20ADS)
- Holds and uses the [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM) internally
- Spawns the "[#Connection Listener Thread](%23Connection%20Listener%20Thread)" 
- Will spawn and cleanup several threads during operation (does not use pool)
- Requires:
	- [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM) to have been initialized
- Returns:
	- Pointer to an ACL instance or NULL on error

```c
acl_hndl *acl_init(aim_hndl *pAIM);
```
## Deconstruction
- Cancels the main "connection listener" thread
- Waits for the thread to join the main thread
- Requires:
	- An existing ACL ([#Initialization](%23Initialization))
- Returns 0 or null parameter

## Internal Threads
### Connection Listener Thread
- Spawned by ACL [#Initialization](%23Initialization)
- Runs until ACL [#Deconstruction](%23Deconstruction)
- Blocks on [AURORA Discovery Service - ADS#Accepting Exchanges](AURORA%20Discovery%20Service%20-%20ADS%23Accepting%20Exchanges)
- Reports failures through the logger
- Spawns a [#Connection Acceptance Thread](%23Connection%20Acceptance%20Thread) when the ADS returns a new connection

### Connection Acceptance Thread
- Creates all server side instances and adds them to the [AURORA Instance Manager - AIM](AURORA%20Instance%20Manager%20-%20AIM)
	- [AURORA Connection Instance - ACI](AURORA%20Connection%20Instance%20-%20ACI)
	- [AURORA Region Manager - ARM](AURORA%20Region%20Manager%20-%20ARM)
	- [AURORA Completion Notification - ACN](AURORA%20Completion%20Notification%20-%20ACN)
- Spawned by [#Connection Listener Thread](%23Connection%20Listener%20Thread)
	- CHANGED April 2, 2026
	- Now spawned using the [AURORA Command Runner - ACR](AURORA%20Command%20Runner%20-%20ACR)
	- Handle is left NULL, socket is placed in the "flags" parameter
	- Can time out if the ACR does not have enough slots/server is busy
	- Changed in PR `refactor-acr`
- Runs until the connection process fails or completes.
- Logs all output/errors to the logger