#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include "devices/block.h"

#define CACHE_BLOCK_SIZE 64

struct cache_entry {
    block_sector_t index;
    int dirty;
    int use;
    uint8_t *data;
    struct list_elem elem;
};

void cache_init(void);
struct cache_entry* cache_get(block_sector_t index);
void cache_update(struct cache_entry *entry,
        int offset, const uint8_t *buffer, int size);
void cache_set(block_sector_t index, const uint8_t *buffer);

#endif
