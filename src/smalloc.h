/*
 * Small object allocator with minimal overhead
 * and O(1) alloc/free performance.
 * Note it is not thread-safe and requires C99
 * Thread safety can be achieved by using separate
 * pools per thread, though.
 */

/* example:

#include "smalloc.h"

typedef struct {
    int x, y, w, h, order;
    uint32_t flags, color;
} my_thingy_t;

smalloc_pool_t my_thingy_pool = SMALLOC_POOL(sizeof(my_thingy_t));

int dothings() {
    my_thingy_t *t = smalloc(&my_thingy_pool);
    ...
}

void alldone() {
    printf("You alloc'd %d things", smalloc_count(&my_thingy_pool));
    smfree_all(&my_thingy_pool);
}

*/
#include <stddef.h>

#ifndef SMALLOC_BLOCK_SIZE
// target block size in bytes
#define SMALLOC_BLOCK_SIZE 4096
#endif

typedef struct smalloc_block_ smalloc_block_t;

struct smalloc_block_ {
    smalloc_block_t *next_block;
    char data[];
};

typedef struct {
    size_t obj_size, block_size;
    char *new_ptr;
    void *free_ptr;
    smalloc_block_t *block;
} smalloc_pool_t;

/*
 * Initialize a smalloc_pool_t statically for the object size desired.
 * Note the smallest actual object size is sizeof(void *)
 */
#define SMALLOC_POOL(object_size)\
    (smalloc_pool_t){ .obj_size = (object_size),\
                      .block_size = SMALLOC_BLOCK_SIZE / (object_size) * (object_size) }

/*
 * sm_prealloc() preallocates a block for the smalloc pool if no free space
 * is available, such as in the case of a fresh pool. If there is already
 * free space available, the call does nothing.
 *
 * Using this is optional since smalloc() will do it automatically, but can
 * be used to guarantee that following call(s) to smalloc() are as
 * consistently fast as possible.
 *
 * returns -1 if the allocation fails, 0 otherwise.
 */
int smprealloc(smalloc_pool_t *pool);

/*
 * smalloc() returns a new object pointer from the pool.
 * In the best case this is simply a pointer increment.
 * If an object has been freed, it will be recycled and returned.
 *
 * If no free block space is available, a new pool block will be allocated.
 * The first call to smalloc() with a fresh pool will always need to allocate
 * a new block. To avoid this, you can call sm_prealloc() beforehand.
 * All objects are zeroed when allocated unless SMALLOC_NOZERO is set.
 */
void *smalloc(smalloc_pool_t *pool);

/*
 * smfree() returns an object pointer created by smalloc to the pool, allowing
 * it to be recycled. This is always a simple O(1) pointer assignment. The
 * value of the object is overwritten thus should not be used again, as in
 * free(). Note smfree() returns no memory to the system.
 *
 * Objects returned to a pool must have been created from the same pool or
 * corruption can occur. For debugging purposes, this can be enforced by
 * setting SMALLOC_DEBUG, though this adds overhead required to check that
 * the pointer is within an active block of the pool.
 */
void smfree(smalloc_pool_t *pool, void *ptr);

/*
 * smfree_all() efficiently frees all of the objects allocated in the pool
 * and and all of the pool's allocated memory blocks.
 */
void smfree_all(smalloc_pool_t *pool);

/*
 * smcount() returns the number of objects currently allocated for the
 * pool. This will never decrease except when smfree_all() is used.
 */
int smalloc_count(smalloc_pool_t *pool);

