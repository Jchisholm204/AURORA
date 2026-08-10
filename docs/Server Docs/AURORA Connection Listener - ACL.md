# ACL
The AURORA Connection Listener module is a server specific connection manager that manages the threading aspect of the [[AURORA Discovery Service - ADS]].
- Init function spawns its on management thread.
- Should only be initialized according to [[AURORA Discovery Service - ADS]] specifications.
- Internally holds [[AURORA Discovery Service - ADS]] and [[AURORA Instance Manager - AIM]]
- Requires:
	- [[AURORA Instance Manager - AIM]] (Initialized Instance)
	- [[AURORA Discovery Service - ADS]]

## Initialization
- Allocates threading data structures (used internally only)
- Allocates an internal [[AURORA Discovery Service - ADS]]
- Holds and uses the [[AURORA Instance Manager - AIM]] internally
- Spawns the "[[#Connection Listener Thread]]" 
- Will spawn and cleanup several threads during operation (does not use pool)
- Requires:
	- [[AURORA Instance Manager - AIM]] to have been initialized
- Returns:
	- Pointer to an ACL instance or NULL on error

```c
acl_hndl *acl_init(aim_hndl *pAIM);
```
## Deconstruction
- Cancels the main "connection listener" thread
- Waits for the thread to join the main thread
- Requires:
	- An existing ACL ([[#Initialization]])
- Returns 0 or null parameter

## Internal Threads
### Connection Listener Thread
- Spawned by ACL [[#Initialization]]
- Runs until ACL [[#Deconstruction]]
- Blocks on [[AURORA Discovery Service - ADS#Accepting Exchanges]]
- Reports failures through the logger
- Spawns a [[#Connection Acceptance Thread]] when the ADS returns a new connection

### Connection Acceptance Thread
- Creates all server side instances and adds them to the [[AURORA Instance Manager - AIM]]
	- [[AURORA Connection Instance - ACI]]
	- [[AURORA Region Manager - ARM]]
	- [[AURORA Completion Notification - ACN]]
- Spawned by [[#Connection Listener Thread]]
	- CHANGED April 2, 2026
	- Now spawned using the [[AURORA Command Runner - ACR]]
	- Handle is left NULL, socket is placed in the "flags" parameter
	- Can time out if the ACR does not have enough slots/server is busy
	- Changed in PR `refactor-acr`
- Runs until the connection process fails or completes.
- Logs all output/errors to the logger