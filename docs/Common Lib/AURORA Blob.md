Standard `typedef` for dealing with anonymous `void*` data.
```c
typedef struct {
    void *data;
    size_t size;
} aurora_blob_t;
```