# AOC
The AURORA Operating Configuration structure defines all high-level client-server configuration for a single connection.
- `typedef opconf_t`
- Must be allocated as a single block. Use its internal allocation function
	- Can be sent over the network without being packed
	- Use `size_t aoc_size(const opconf_t*)` to get the size of the block
- Options here are linked to a subset of the options in the [[AURORA Client Configuration]]

## Initialization
- AOC must be a contiguous block of memory
- Use the AOC initialization function
- Requires:
	- The persistent checkpoint path location
- Returns:
	- A configuration struct pointer, filled with default values + the persistent path argument 
	- (only free returned memory using [[#Deconstruction]])

```c
static inline opconf_t *aoc_alloc(char *persistent_path);
```
## Deconstruction
- Used to free an AOC structure
- Since AOC is a single block of memory, 
	- it technically does not need its own free function,
	- one is used for consistency

```c
static inline void aoc_free(struct aurora_operating_configuration **ppConfig);
```

