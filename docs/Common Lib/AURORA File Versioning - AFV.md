# AFV
The AURORA File Versioning system encapsulates all metadata operations, as well as checkpoint reads and writes.
- The AFV handle may be created or destroyed anytime, multiple times
- Destruction of the AFV handle automatically closes all active references
- Repeated construction/deconstruction of the AFV may cause excess system noise
- The `afv_metadata_t` structure dynamically encapsulates all metadata
- The `afv_file_hndl` structure holds checkpoint data, it can be read from and written to in chuncks

## Instance Creation
- Creates the metadata handler instance based on identifying information
- Validates the persistent checkpoint path
- Loads the latest checkpoint metadata from the disk, if it exists
- Returns:
	- `NULL` on error
	- An `afv_hndl`, can be used to perform metadata lookups, read and write checkpoint files

```c
afv_hndl *afv_create_instance(uint64_t rank, uint64_t group_id,
                              int64_t group_size, char *persistent_path,
                              bool with_ec);
```

## Deconstruction
- Free's the internal metadata structure
- Destroys the AFV instance

```c
void afv_destroy_instance(afv_hndl **ppHndl);
```

## Metadata Lookup
- Usefull to read/write metadata files to/from the disk
### Load Metadata from Disk
- Attempts to load versioned metadata from the disk
- The Checkpoint Version may be set to `-1` to return the highest version
- The Checkpoint Name may be set to `NULL` acheive the `*` effect
- Requires:
	- An AFV instance
- Returns:
	- `NULL` If the metadata requested could not be loaded
	- `afv_metadata_t*` on success, free with [#Destroy Metadata Block](#Destroy%20Metadata%20Block)

```c
const afv_metadata_t *afv_get_metadata_versioned(afv_hndl *pHndl,
	                                             int64_t version,
                                                 const char *name);
```
### Write Metadata to Disk
- Verifies checkpoint metadata before writing to disk
- Writes an `afv_metadata_t*` to the persistent storage path
- After writing, the metadata pointer becomes owned by the AFV system
	- Pointer will be freed on the next replacement, or during [#Deconstruction](#Deconstruction)
- Requires:
	- An AFV instance
	- A valid `afv_metadata_t*` (checked with [#Verify Metadata Validity](#Verify%20Metadata%20Validity))
- Returns: 
	- An AFV metadata verification error, or;
	- `eAFV_VERIF_OK` on Success

```c
eAFV_verif afv_write_metadata(afv_hndl *pHndl,
	                          afv_metadata_t *const pMetadata);
```
### Get Cached Metadata
- Returns a read only pointer to the internally cached metadata, either the latest written checkpoint, or the checkpoint read from the disk on startup
- *Note:* The AFV does not perform syncronization, ie, metadata is stored per AFV instance.
- Requires:
	- An initialized AFV Instance (with or without a write completed)
- Returns:
	- `NULL` if the instance pointer was bad
	- A valid, read only `afv_metadata_t*`

```c
const afv_metadata_t *afv_get_metadata(afv_hndl *pHndl);
```
## Metadata Utilities
### Create Metadata Block
- Creates an empty metadata block pointer
- This function must be used to create`afv_metadata_t` pointers (memory must be contiguous)
- Requires:
	- The number of region slots within the metadata block
- Returns:
	- An empty, writable, `afv_metadata_t*`, free only with [#Destroy Metadata Block](#Destroy%20Metadata%20Block)

```c
afv_metadata_t *afv_create_metadata(size_t n_regions);
```
### Destroy Metadata Block
- Free's an `afv_metadata_t` pointer
- Must be used instead of the typical `free(void*)`

```c
void afv_destroy_metadata(afv_metadata_t **ppHndl);
```
### Calculate Expected Metadata Size
- Calculates the expected block size (in bytes) of a metadata pointer
- Includes the size of the metadata structure, and included data
- Signature similar to [#Create Metadata Block](#Create%20Metadata%20Block)
```c
size_t afv_metadata_size(size_t n_regions);
```
### Read Metadata Block Size
- Reads the metadata size from a raw block pointer (this is the size from disk)
- Requires:
	- A raw pointer, containing valid checkpoint metadata
- Returns:
	- The size previously from [#Calculate Expected Metadata Size](#Calculate%20Expected%20Metadata%20Size), stored within a raw block

```c
size_t afv_metadata_ptr_size(void *);
```
### Initialize Metadata from Block
- Typecasts raw block pointers into `afv_metadata_t*` 
- This function does not allocate new memory, it just properly sets up internal pointers
- Requires:
	- A block pointer (`void*`)
- Returns:
	- The block pointer as a `afv_metadata_t`

```c
afv_metadata_t *afv_metadata_ptr_init(void *);
```
### Verify Metadata Validity
- Verifies an `afv_metadata_t` block contains valid metadata
- Requirements:
	- Version is non-negative
	- Internal Key Match
	- Internal size matches expected size of block
- Returns:
	- `eAFV_VERIF_OK` or a verification error

```c
eAFV_verif afv_metadata_verify(const afv_metadata_t *pMetadata);
```
### Match Metadata to Region List
- Matches regions within a metadata block to an ARM region list (list of AMR handles)
- `match_map_list`: 
	- Mapping: Index(Metadata Region) -> Index(ARM)
	- Maps indexes in the metadata region list to the ARM region list
	- `region_list_index = match_map_list[metadata_index]`
	- `region = region_list[match_map_list[metadata_index]]`
- Requires:
	- `match_map_list` to be `NULL` (ignore), or contain at least `n_regions` slots
	- A valid Metadata pointer
	- A valid region list from the ARM
- Returns:
	- Verification match status
	- Metadata to region list mapping (if `match_map_list` is not `NULL`)

```c
eAFV_verif afv_metadata_match(const afv_metadata_t *pMetadata,
                              const amr_hndl *const region_list,
                              size_t n_regions, size_t *match_map_list);
```
## File Operations
### Open File
- Opens a Checkpoint file for reading or writing
- Files can only be opened with one mode at a time
- Requires:
	- An AFV instance
	- The name and version of the checkpoint to load (will return `NULL` if not valid)
- Returns:
	- An AFV file handle or `NULL`

```c
afv_file_hndl *afv_file_open_w(afv_hndl *pAFV, int64_t version,
                               const char *name);
```

```c
afv_file_hndl *afv_file_open_r(afv_hndl *pAFV,
                               const afv_metadata_t *pMetadata);
```

### Close File
- Closes an open AFV file

```c
eAFV_file_error afv_file_close(afv_file_hndl **ppHndl);
```
### File Seek
- Wrapper around `fseek(file, seekptr, SEEK_SET)`

```c
eAFV_file_error afv_file_seek(afv_file_hndl *pHndl, size_t seekptr);
```
### File Jump
- Wrapper around `fseek(file, jsize, SEEK_CUR)`

```c
eAFV_file_error afv_file_jump(afv_file_hndl *pHndl, int64_t jsize);
```
### File Write
- Wrapper around `fwrite_unlocked`

```c
eAFV_file_error afv_file_write(afv_file_hndl *pHndl, const void *restrict data,
                               size_t size);
```
### File Read
- Wrapper around `fread_unlocked`

```c
eAFV_file_error afv_file_read(afv_file_hndl *pHndl, void *data,
                              size_t size);
```