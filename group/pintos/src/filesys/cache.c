#include "filesys/cache.h"
#include "filesys/filesys.h"
#include <string.h>
#include "threads/malloc.h"

static struct list cached_blocks;
static int cache_size;
static struct list_elem *cache_pointer;

static void write_back(struct cache_entry *ce);
static void cache_remove(struct cache_entry *ce);
static void clock_alg_replace(void);
static bool index_cmp (const struct list_elem *a,
        const struct list_elem *b, void *aux UNUSED);

void
cache_init()
{
    list_init(&cached_blocks);    
    cache_size = 0;
    cache_pointer = NULL;
}

struct cache_entry* cache_get(block_sector_t index)
{
    struct list_elem *e = NULL;
    struct cache_entry *ce = NULL;

    for (e = list_begin(&cached_blocks);
            e != list_end(&cached_blocks);
            e = list_next(e))
    {
        ce = list_entry(e, struct cache_entry, elem);
        if (ce->index == index) {
            ce->use = 1;
            return ce;
        }
        if (ce->index > index)
            break;
    }
    return NULL;
}
void cache_update(struct cache_entry *entry,
        int offset, const uint8_t *buffer, int size)
{
    memcpy(entry->data + offset, buffer, size); 
    entry->dirty = 1;
    entry->use = 1;
}
void cache_set(block_sector_t index, const uint8_t *buffer)
{
    struct cache_entry *ce = malloc(sizeof(struct cache_entry));
    uint8_t *data = malloc(sizeof(uint8_t) * BLOCK_SECTOR_SIZE);

    memcpy(data, buffer, BLOCK_SECTOR_SIZE);
    ce->index = index;
    ce->dirty = 0;
    ce->use = 1;
    ce->data = data;
    list_insert_ordered(&cached_blocks, &ce->elem,
            index_cmp, NULL);
    cache_size++;
    if (cache_size > CACHE_BLOCK_SIZE)
        clock_alg_replace();
}
bool index_cmp (const struct list_elem *a,
        const struct list_elem *b, void *aux UNUSED)
{
    struct cache_entry *cea = list_entry(a, struct cache_entry,
            elem);
    struct cache_entry *ceb = list_entry(b, struct cache_entry,
            elem);
    return cea->index < ceb->index;
}
void clock_alg_replace() 
{
    struct cache_entry *ce = NULL;

    if (cache_pointer == NULL ||
            cache_pointer == list_end(&cached_blocks))
        cache_pointer = list_begin(&cached_blocks);
    while (1) {
        ce = list_entry(cache_pointer, struct cache_entry,
                elem);
        if (ce->dirty)
            write_back(ce);
        if (!ce->use) {
            cache_pointer = list_next(cache_pointer);
            cache_remove(ce);
            cache_size--;
            return;
        }
        ce->use = 0;
        cache_pointer = list_next(cache_pointer);
        if (cache_pointer == list_end(&cached_blocks))
            cache_pointer = list_begin(&cached_blocks);
    }
}
void cache_remove(struct cache_entry *ce)
{
    list_remove(&ce->elem);
    free(ce->data);
    free(ce);
}
void write_back(struct cache_entry *ce)
{
    block_write(fs_device, ce->index, ce->data);
    ce->dirty = 0;
}
