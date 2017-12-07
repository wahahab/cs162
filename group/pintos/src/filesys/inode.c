#include "filesys/inode.h"
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

void inode_read_ahead(void *aux);
bool inode_extend(struct inode *inode, off_t size);
void inode_free(struct inode *inode);
bool inode_create_real(block_sector_t sector, off_t length,
        bool isdir);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  block_sector_t *buffer;
  block_sector_t *buffer2;
  block_sector_t retval = -1;
  off_t level1_ofs;
  off_t level2_ofs;
  off_t i = pos / BLOCK_SECTOR_SIZE;

  ASSERT (inode != NULL);
  // init buffers
  buffer = malloc(sizeof(block_sector_t) * BLOCK_SECTOR_SIZE);
  buffer2 = malloc(sizeof(block_sector_t) * BLOCK_SECTOR_SIZE);
  // Load level 1 buffer
  if (i >= 12) {
    block_sector_t level1_idx = inode->data.blocks[12];
    block_read(fs_device, level1_idx, buffer);
  }
  if (pos >= inode->data.length)
      retval = -1;
  else if (i < 12) // size in direct block range
  {
    retval = inode->data.blocks[i];
  }
  // indirect block range
  else if (i < 12 + 64)
  {
    retval = (block_sector_t) buffer[i - 12];
  }
  // double indirect block range
  else if (i < 12 + 64 + 64 * 128)
  {
    level2_ofs = (i - 12 - 64) % 128;
    level1_ofs = (i - 12 - 64) / 128 + 64;
    block_read(fs_device, buffer[level1_ofs], buffer2);
    retval = (block_sector_t) buffer2[level2_ofs];
  }
  free(buffer);
  free(buffer2);
  return retval;
  //if (pos < inode->data.length)
  //  return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  //else
  //  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  cache_init();
}

bool
inode_create(block_sector_t sector, off_t length)
{
    return inode_create_real (sector, length, false); 
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create_real (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  struct inode inode;
  bool success = 0;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode == NULL)
      return false;
  memset(disk_inode, 0, sizeof *disk_inode);
  // create a pseudo-inode
  inode.sector = sector;
  inode.data = *disk_inode;
  inode.data.isdir = isdir;
  success = inode_extend(&inode, length);
  free(disk_inode);
  return true;
  

      // if (free_map_allocate (sectors, &disk_inode->start))
      //   {
      //     block_write (fs_device, sector, disk_inode);
      //     cache_set(sector, (uint8_t *) disk_inode);
      //     if (sectors > 0)
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;

      //         for (i = 0; i < sectors; i++) {
      //           block_write (fs_device, disk_inode->start + i, zeros);
      //           cache_set(disk_inode->start, (uint8_t *)zeros);
      //         }
      //       }
      //     success = true;
      //   }
      // free (disk_inode);
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  struct cache_entry *ce;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  // if ((ce = cache_get(inode->sector)) != NULL) {
  //   memcpy(&inode->data, ce->data, BLOCK_SECTOR_SIZE);
  // }
  // else {
  //   block_read (fs_device, inode->sector, &inode->data);
  //   cache_set(inode->sector, (uint8_t *) &inode->data);
  // }
  block_read(fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

void
inode_free(struct inode *inode)
{
    struct inode_disk *disk_inode = &inode->data;
    off_t num_sectors = bytes_to_sectors(disk_inode->length);
    off_t i;
    off_t level1_ofs = 0;
    off_t level2_ofs;
    block_sector_t *level1_buffer;
    block_sector_t *level2_buffer;

    level1_buffer = malloc(sizeof(block_sector_t) * BLOCK_SECTOR_SIZE);
    level2_buffer = malloc(sizeof(block_sector_t) * BLOCK_SECTOR_SIZE);
    memset(level1_buffer, 0, sizeof(block_sector_t) * BLOCK_SECTOR_SIZE);
    memset(level1_buffer, 0, sizeof(block_sector_t) * BLOCK_SECTOR_SIZE);
    if (num_sectors > 12) {
        block_read(fs_device, disk_inode->blocks[12], level1_buffer);
    }
    for (i = 0 ; i < num_sectors ; i ++) {
        if (i < 12) {
            free_map_release(disk_inode->blocks[i], 1);
        }
        else if (i < 12 + 64) {
            free_map_release(level1_buffer[i - 12], 1);
        }
        else if (i < 12 + 64 + 64 * 128) {
            level2_ofs = (i - 12 - 64) % 128;
            level1_ofs = (i - 12 - 64) / 128 + 64;
            if (level2_ofs == 0)
                block_read(fs_device, level1_buffer[level1_ofs], level2_buffer);
            free_map_release(level2_buffer[level2_ofs], 1);
            if (level2_ofs == 127)
                free_map_release(level1_buffer[level1_ofs], 1);
        }
    }
    if (num_sectors > 12 + 64)
        free_map_release(level1_buffer[level1_ofs], 1);
    free(level1_buffer);
    free(level2_buffer);
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          inode_free(inode);
          // free_map_release (inode->sector, 1);
          // free_map_release (inode->data.start,
          //                  bytes_to_sectors (inode->data.length));
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

void inode_read_ahead(void *aux) {
    block_sector_t index = *(int *)aux;
    struct cache_entry *ce = cache_get(index + 1);
    uint8_t buffer[BLOCK_SECTOR_SIZE];
    
    if (ce == NULL) {
        block_read(fs_device, index + 1, buffer);
        cache_set(index + 1, buffer);
    }
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      struct cache_entry *ce = cache_get(sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (ce != NULL) {
          memcpy (buffer + bytes_read, ce->data + sector_ofs,
                  chunk_size);
      }
      else {
        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
          {
            /* Read full sector directly into caller's buffer. */
            block_read (fs_device, sector_idx, buffer + bytes_read);
            cache_set(sector_idx, buffer + bytes_read);
          }
        else
          {
            /* Read sector into bounce buffer, then partially copy
               into caller's buffer. */
            if (bounce == NULL)
              {
                bounce = malloc (BLOCK_SECTOR_SIZE);
                if (bounce == NULL)
                  break;
              }
            block_read (fs_device, sector_idx, bounce);
            cache_set(sector_idx, bounce);
            memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
          }
      }

      // read ahead
      thread_create("read_ahead", PRI_DEFAULT,
              inode_read_ahead, &sector_idx);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

// extend inode's size to (size)
bool inode_extend(struct inode *inode, off_t size) {
    off_t current_sectors = bytes_to_sectors(inode_length(inode));
    off_t to_sectors = bytes_to_sectors(size);
    off_t i;
    off_t level1_ofs = 0;
    off_t level2_ofs;
    block_sector_t *level1_buffer;
    block_sector_t *level2_buffer;
    static char *zeros;
    block_sector_t level1_idx;
    block_sector_t level2_idx;
    block_sector_t sector_idx;
    struct inode_disk *disk_inode = &inode->data;
    struct cache_entry *ce;

    level1_buffer = malloc(BLOCK_SECTOR_SIZE);
    level2_buffer = malloc(BLOCK_SECTOR_SIZE);
    zeros = malloc(sizeof(char) * BLOCK_SECTOR_SIZE);
    // initial level2_buffer and level1_buffer
    memset(zeros, 0, sizeof(char) * BLOCK_SECTOR_SIZE);
    memset(level1_buffer, 0, BLOCK_SECTOR_SIZE);
    memset(level2_buffer, 0, BLOCK_SECTOR_SIZE);
    // load level 1 block first
    if (current_sectors > 12)
        block_read(fs_device, inode->data.blocks[12], level1_buffer);
    // if level 1 block isn't allocate but we need it
    // then create it
    else if (to_sectors > 12) {
        if (!free_map_allocate(1, &level1_idx)) {
            free(level1_buffer);
            free(level2_buffer);
            free(zeros);
            return false;
        }
        block_write(fs_device, level1_idx, zeros);
        inode->data.blocks[12] = level1_idx;
    }
    // check if we need to load current max level2 block
    if (current_sectors > (12 + 64)) {
        off_t ofs = (current_sectors - 1 - 12 - 64) / 128 + 64;
        block_read(fs_device, level1_buffer[ofs], level2_buffer);
    }
    // allocate blocks
    for (i = current_sectors ; i < to_sectors ; i++) {
        if (!free_map_allocate(1, &sector_idx)) {
            free(level1_buffer);
            free(level2_buffer);
            free(zeros);
            return false;
        }
        block_write(fs_device, sector_idx, zeros);
        if (i < 12)
            disk_inode->blocks[i] = sector_idx;
        else if (i < 12 + 64)
            level1_buffer[i - 12] = sector_idx;
        else if (i < 12 + 64 + 64 * 128) {
            level1_ofs = (i - 12 - 64) / 128 + 64;
            level2_ofs = (i - 12 - 64) % 128;
            if (level2_ofs == 0) {
                if (!free_map_allocate(1, &level2_idx)) {
                    free(level1_buffer);
                    free(level2_buffer);
                    free(zeros);
                    return false;
                }
                level1_buffer[level1_ofs] = level2_idx;
                memset(level2_buffer, 0, BLOCK_SECTOR_SIZE);
            }
            level2_buffer[level2_ofs] = sector_idx;
            if (level2_ofs == 127) {
                block_write(fs_device, level1_buffer[level1_ofs], level2_buffer);
            }
        }
        else {
            free(level1_buffer);
            free(level2_buffer);
            free(zeros);
            return false;
        }
    }
    // save level 2 buffer to disk
    if (to_sectors > 12 + 64)
        block_write(fs_device, level1_buffer[level1_ofs], level2_buffer);
    // save level 1 buffer to disk
    if (to_sectors > 12)
        block_write(fs_device, disk_inode->blocks[12], level1_buffer);
    // extend success, save inode to disk
    inode->data.length = size;
    disk_inode->magic = INODE_MAGIC;
    block_write(fs_device, inode->sector, disk_inode);
    free(level1_buffer);
    free(level2_buffer);
    free(zeros);
    return true;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  // If there is no enough space in inode, allocate first
  // if extend size fail, return 0
  if (offset + size > inode_length(inode)
          && !inode_extend(inode, offset + size))
        return 0;

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      struct cache_entry *ce = cache_get(sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (ce != NULL) {
          cache_update(ce, sector_ofs, buffer + bytes_written,
                  chunk_size);
      }
      else {
        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
          {
            /* Write full sector directly to disk. */
            block_write (fs_device, sector_idx, buffer + bytes_written);
            cache_set(sector_idx, buffer + bytes_written);
          }
        else
          {
            /* We need a bounce buffer. */
            if (bounce == NULL)
              {
                bounce = malloc (BLOCK_SECTOR_SIZE);
                if (bounce == NULL)
                  break;
              }

            /* If the sector contains data before or after the chunk
               we're writing, then we need to read in the sector
               first.  Otherwise we start with a sector of all zeros. */
            if (sector_ofs > 0 || chunk_size < sector_left)
              block_read (fs_device, sector_idx, bounce);
            else
              memset (bounce, 0, BLOCK_SECTOR_SIZE);
            memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
            block_write (fs_device, sector_idx, bounce);
            cache_set(sector_idx, bounce);
          }
      }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
