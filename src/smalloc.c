#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "smalloc.h"

#ifndef SMALLOC_MALLOC
#ifdef SMALLOC_NOZERO
    #define SMALLOC_MALLOC(sz) malloc(sz)
#else
    #define SMALLOC_MALLOC(sz) calloc(1, sz)
#endif
#endif
#ifndef SMALLOC_FREE
    #define SMALLOC_FREE(p) free(p)
#endif

static smalloc_block_t *block_alloc(smalloc_pool_t *pool) {
    assert(pool->obj_size >= sizeof(void *));
    assert(pool->obj_size <= SMALLOC_BLOCK_SIZE);
    smalloc_block_t *block = SMALLOC_MALLOC(SMALLOC_BLOCK_SIZE);
    if (block == NULL) return NULL;
    block->next_block = pool->block;
    pool->block = block;
    pool->new_ptr = block->data;
    return block;
}

int smprealloc(smalloc_pool_t *pool) {
    if (!pool->block || (pool->new_ptr - pool->block->data
                         > SMALLOC_BLOCK_SIZE - pool->obj_size)) {
        return 0;
    }
    return block_alloc(pool) != NULL ? 0 : -1;
}

void *smalloc(smalloc_pool_t *pool) {
    void *obj;
    if (pool->free_ptr) {
        obj = pool->free_ptr;
        pool->free_ptr = *(void **)obj;
#ifndef SMALLOC_NOZERO
        memset(obj, 0, pool->obj_size);
#endif
        return obj;
    }
    if (smprealloc(pool) == 0) {
        obj = pool->new_ptr;
        pool->new_ptr += pool->obj_size;
        return obj;
    }
    return NULL;
}

void smfree(smalloc_pool_t *pool, void *ptr) {
    assert(pool->block);
    assert(ptr);
    // TODO implement SMALLOC_DEBUG
    *(void **)ptr = pool->free_ptr;
    pool->free_ptr = ptr;
}

static void free_block(smalloc_block_t *block) {
    if (!block) return;
    free_block(block->next_block);
    SMALLOC_FREE(block);
}

void smfree_all(smalloc_pool_t *pool) {
    free_block(pool->block);
    pool->block = NULL;
    pool->new_ptr = NULL;
    pool->free_ptr = NULL;
}

int smalloc_count(smalloc_pool_t *pool) {
    smalloc_block_t *block = pool->block;
    int n_blocks = 0;
    while (block) {
        block = block->next_block;
        n_blocks++;
    }
    return (SMALLOC_BLOCK_SIZE / pool->obj_size) * n_blocks;
}

