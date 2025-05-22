# Reference for common data structures used throughout the project

In May 2025 we began to remove GLib dependencies from this project. Thus, we need to find replacements for various GLib objects used in the project, especially `GHashTable` and `GThreadPool`. This document is a brief introduction on how to write new code using replacement datastructure libraries.

## Hash Table

- Type: Header-only C library
- Use: Generic (non-cache related)
- Path: *include/libCacheSim/hashmap.h*

This is a generic hash table imported from [sheredom/hashmap.h](https://github.com/sheredom/hashmap.h). It is used everywhere except with cache objects, where there exists an optimised hashtable implementation.

#### Structs

```c
struct hashmap_element_s {
  const void *key;
  void *data;
}
```

This structure is a representation of the hash table element.



```c
struct hashmap_create_options_s {
  hashmap_hasher_t hasher;
  hashmap_comparer_t comparer;
  hashmap_uint32_t initial_capacity;
}
```

This structure needs to be populated if custom hasher/comparer is desired. If it's not fully populated, the default will be used.




```c
hashmap_uint32_t hasher(const hashmap_uint32_t seed, const void *const s,
                        const hashmap_uint32_t len)
```

- Signature of custom hasher.

  - `seed`: random seed, if **reproducible** randomness is desired.

  - `s`: key of the element to be stored in the hash table.

  - `len`: length of `s`. By design `s` is a C string. If it is not, then the use of `len` can be user-defined (e.g. ignored.)

```c
int obj_id_comparer(const void *const a, const hashmap_uint32_t a_len,
                    const void *const b, const hashmap_uint32_t b_len)
```

- Signature of custom comparer.
  - Return **0** on equal, **1** on non-equal (regardless of `a` being greater of lesser.)

#### Methods

```c
int hashmap_create(const hashmap_uint32_t initial_capacity,
                   struct hashmap_s *const out_hashmap)
```

- Create a hash table using all defaults.
  - `out_hashmap`: hash table structure. It does not need to be on the heap, but it MUST be initialized before this call.
  
    

```c
int hashmap_create_ex(struct hashmap_create_options_s options,
                      struct hashmap_s *const out_hashmap)
```

- Create a hashmap using customised options.

  

```c
int hashmap_put(struct hashmap_s *const hashmap, const void *const key,
                const hashmap_uint32_t len, void *const value)
```

- Put a key-value pair into the hash table. If the key exists, the value will be updated.

  - `key`: by design it's a C string, but it can be something else as long as a custom hasher is provided.

    **MUST** exist during the entire lifecycle of the hash table. It will **NOT** automatically be copied.

  - `len`: the length of the key if it's a C string. If the key is something else, it's user-defined.

  - `value`: it can be anything, but like the key, it **MUST** exist during the entire lifecycle of the hash table.

    

```c
void *hashmap_get(const struct hashmap_s *const hashmap,
                  const void *const key, const hashmap_uint32_t len)
```

- Get a **value** from the hash table.

  

```c
int hashmap_remove(struct hashmap_s *const hashmap, const void *const key,
                   const hashmap_uint32_t len)
```

- Remove a key-value pair from the hash table.

  

```c
int hashmap_iterate(const struct hashmap_s *const hashmap, int (*iterator)
                    (void *const context, void *const value), void *const context)
```

- Iterate through all **values** from the hash table.

  - `iterator`: function pointer to pass to the iterator. Can pass in an additional context as the first parameter.

    To exit from iteration at any time, return **non-zero**. To continue, return **0**.

    

```c
int hashmap_iterate_pairs(struct hashmap_s *const hashmap, int (*iterator)
                          (void *const, struct hashmap_element_s *const),
                          void *const context)
```

- Iterate through all key-value pairs from the hash table.

  - `iterator`: function pointer to pass to the iterator. Can pass in an additional context as the first parameter.

    To remove the current element (**only removes record, not memory**) and continue iteration, return **-1**.

    To end iteration, return a **positive integer**. To continue, return **0**.

    

```c
void hashmap_destroy(struct hashmap_s *const hashmap)
```

- Destroys the hash table.

  - If the hash table is created on heap, it is still needed to call `free` on `hashmap` to free its memory.

    

#### Example

```c
struct hashmap_create_options_s options = { .initial_capacity = 16 };
struct hashmap_s hashmap;
hashmap_create(options, &hashmap);

char* value = (char *)malloc(sizeof(char) * 5);
strcpy(value, "value");
if (hashmap_put(&hashmap, "key", 3, value) != 0)
    printf("encountered unknown error in hashmap_put()\n");

char* content = (char *)hashmap_get(&hashmap, "key", 3);
printf("content is %s\n", content);

hashmap_destroy(&hashmap); free(value);
free(content); // double free
```



## Thread Pool

- Type: C library
- Use: Generic
- Path: *utils/include/threadpool.h*
- Link: Add *utils* to `target_link_libraries` in CMakeLists

This is a simple fixed-size thread pool adapted from https://nachtimwald.com/2019/04/12/thread-pool-in-c, which uses semantics similar to that of GLib. It currently does not support adjusting priority of jobs, etc.

#### Methods

```c
bool threadpool_create(threadpool_t *tm, size_t num_threads)
```

- Create a thread pool

  - `tm`: structure of thread pool. **MUST** be pre-allocated on the heap.

    

```c
void threadpool_destroy(threadpool_t *tm)
```

- Destroys the thread pool
  - The memory associated with `tm` is also freed, so calling `free()` on `tm` again will cause double free!



```c
bool threadpool_push(threadpool_t *tm, void (*job)(void *arg1, void *arg2),
                     void *arg, void *arg2)
```

- Pushes a job to the execution queue of the thread pool. 
  - `job`: a function that accepts 2 arguments. It is easy to modify this library to make `job` accept more arguments.



#### Example

```c
// Adapted from https://nachtimwald.com/2019/04/12/thread-pool-in-c/
void worker(void *arg1, void* arg2)
{
    int *old = arg1;
    int *new = arg2;

    *new += 1000;
    printf("tid=%p, old=%d, new=%d\n", pthread_self(), *old, *new);

    if (*old % 2)
        usleep(100000);
}

int main(int argc, char **argv)
{
    threadpool_t *tm = (threadpool_t *)malloc(sizeof(threadpool_t));
    int     *vals;
    size_t   i;

    tm   = threadpool_create(num_threads);
    vals = calloc(num_items, sizeof(*vals));

    for (i=0; i<num_items; i++) {
        vals[i] = i;
        threadpool_push(tm, worker, vals+i, vals+i);
    }

    // waits for all vals to be updated
    threadpool_wait(tm);

    for (i=0; i<num_items; i++) {
        printf("%d\n", vals[i]);
    }

    free(vals);
    threadpool_destroy(tm);
    free(tm); // double free
}
```



## Array

TBC



## Queue

TBC



## Misc

TBD
