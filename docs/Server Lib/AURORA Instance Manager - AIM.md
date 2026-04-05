# AIM
The AURORA Instance Manager is a lock free FIFO queue that emulates a mutex for [[AURORA Connection Instance - ACI]]'s on the server.
- AIM entries group ACI's with related entities
	- [[AURORA Connection Instance - ACI|ACI's]] can only be accessed by one thread at a time, only one thread can hold an AIM entry at a time
	- AIM entries can be popped or returned to the instance manager to "get/release" the mutex
- Separate memory is used for the queue and handles
	- The handle memory is unaffected by queue operations
- Does not access internal entry memory/Entry data is never automatically freed
- Requires:
	- Structures from [[AURORA Connection Instance - ACI|ACI]], [[AURORA Region Manager - ARM|ARM]], and[[AURORA Completion Notification - ACN|ACN]]

## Initialization
- Initializes internal queue and handle memory
- All internal memory is fixed on initialization
- Does not initialize any entry memory (see [[AURORA Connection Listener - ACL#Connection Acceptance Thread|ACL Connection Acceptance]])
- Requires:
	- `max_workers`: The maximum number of connections the server can handle simultaneously 
- Returns: An AIM handle or NULL on error

```c
aim_hndl *aim_init(size_t max_workers);
```

## Deconstruction
- Destroys all memory associated with the AIM
	- Does not de-initialize instance/entry handles, only deletes handle memory
	- Instances/entries need to be externally destroyed
- Requires:
	- An AIM handle
- Returns 0 or NULL param error

```c
int aim_finalize(aim_hndl **ppHndl);
```


## Standard Interface
- Describes all functions related to AIM entries
- `__reserved` portion may be used internally by AIM. This memory should not be accessed externally
- Contains an [[AURORA Connection Instance - ACI]] handle + all associated connection objects
- Used only in server side components

```c
struct aurora_instance_manager_entry {
    aci_hndl *pACI;
    acn_hndl *pACN;
    arm_hndl *pARM;
    union {
#ifdef AIM_INTERNAL
        struct {
            size_t instance_index;
            atomic_size_t references;
        };
#endif
        uint64_t __reserved[2];
    };
};
```
### Add Entry
- Uses a new slot within the entries list
- Memory returned by this function should only be deallocated with [[#Remove Entry]]
- Multithreaded safe (do not need to use mutex)
- Entries must be [[#Enqueue Entry|Enqueued]] after creation for them to enter the progress mechanism
- Requires:
	- An initialized AIM handle
- Returns: An uninitialized AIM entry

```c
aim_entry_t *aim_add_entry(aim_hndl *pHndl);
```
### Remove Entry
- Free function for the memory returned by [[#Add Entry]]
- Does not free internal entry data
- Returns the entry memory back to an internal pool
- Requires:
	- An initialized AIM handle
	- A previously allocated handle from [[#Add Entry]]
- Returns: 0 or NULL parameter error

```c
int aim_remove_entry(aim_hndl *pHndl, aim_entry_t *pEntry);
```
### Enqueue Entry
- Enqueue an entry into the processing queue
- Equivalent to releasing the connection object's mutex
- Requires:
	- An initialized AIM handle
	- An Initialized, valid, and ACTIVE AIM entry
- Returns: 0 or NULL parameter

```c
int aim_enqueue(aim_hndl *pHndl, aim_entry_t *pEntry) {
```
### Dequeue Entry
- Dequeue an entry from the processing queue
- Equivalent to acquiring the connection object's mutex
- Requires:
	- An Initialized AIM handle
	- The AIM processing queue to not be empty
- Returns:  
	- The next entry in line for processing
	- NULL if the processing queue is empty (or on error)

```c
aim_entry_t *aim_dequeue(aim_hndl *pHndl);
```