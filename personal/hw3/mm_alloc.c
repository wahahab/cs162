/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
/* Your final implementation should comment out this macro. */
#define MM_USE_STUBS

struct block_meta* head;
struct block_meta* tail;

void* mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    struct block_meta* meta;
    void *curb = sbrk(0);
    struct block_meta* prev = NULL;

    for(meta = head; meta != NULL && (!meta->free || meta->size < size); meta = meta->next, prev = meta);
    if (meta == NULL){
        if (tail != NULL && (tail->data + sizeof(meta) * 2 + tail->size + size > curb)) {
            if (*(int*)sbrk((tail->data + sizeof(meta) * 2 + tail->size + size) - curb) == -1)
                return NULL;
        }
        if (prev == NULL) {
            sbrk(sizeof(struct block_meta) + size);
            meta = (struct block_meta*) curb;
        } else {
            meta = (struct block_meta*) prev->data + prev->size; 
            prev->next = meta;
        }
        meta->prev = prev;
        meta->next = NULL;
        tail = meta;
        if (head == NULL)
            head = meta;
        meta->data = (void*) meta + meta->size;
    }
    else {
        memset(meta->data, 0, meta->size);
        // split the block if it's too big
        if (meta->size > size * 2) {
            struct block_meta* new_meta = (struct block_meta*)(meta->data + size);

            new_meta->free = 1;
            new_meta->size = 0;
            new_meta->next = meta->next;
            new_meta->prev = meta;
            new_meta->data = (void*) new_meta + sizeof(struct block_meta);
            meta->next = new_meta;
            if (meta->next != NULL)
                meta->next->prev = new_meta;
        }
    }
    meta->size = size;
    meta->free = 0;
    return meta->data;
}

void* mm_realloc(void* ptr, size_t size)
{
    struct block_meta* meta = (struct block_meta*) ptr;

    if (meta == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (meta->next != NULL && size > meta->size)
        return NULL;
    else if (meta->next == NULL) {
        if (meta->prev != NULL) {
            meta->prev->next = NULL;
            tail = meta->prev;
        }
        else {
            head = NULL;
            tail = NULL;
        }
        return mm_malloc(size);
    }
    meta->size = size;
    return meta->data;
}

void mm_free(void* ptr)
{
    if (ptr == NULL)
        return ;
    struct block_meta* meta = (struct block_meta*) (ptr - sizeof(struct block_meta));
    struct block_meta* pt = meta;

    meta->free = 1;
    // merge right
    while (pt->next != NULL && pt->next->free) {
        pt->size += (sizeof(struct block_meta) + pt->next->size);
        pt->next = pt->next->next;
        if (pt->next != NULL)
            pt->next->prev = pt;
        pt = pt->next;
    }
    // merge left
    while (pt->prev != NULL && pt->prev->free) {
        pt->prev->size += (sizeof(struct block_meta) + pt->size);
        pt->prev->next = pt->next;
        if (pt->next != NULL)
            pt->next->prev = pt->prev;
        pt = pt->prev;
    }
}
