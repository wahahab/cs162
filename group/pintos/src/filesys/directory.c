#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"


static bool dir_isempty(struct dir*);

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create_real (sector,
          entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

bool
dir_isempty(struct dir *dir)
{
  off_t ofs;
  struct dir_entry e;

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use)
        return false;
  return true;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  if (inode->data.isdir && !dir_isempty(dir_open(inode)))
      goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

bool
to_dir_path(char *path)
{
    size_t i;
    char *prev = NULL;
    char *cur = NULL;
    char c;

    // make sure path ends with /
    if (*(path + strlen(path) - 1) != '/')
        strlcat(path, "/", sizeof(char));
    for (i = 0 ; i < strlen(path) ; i ++)
    {
        c = *(path + i);
        if (c == '/') {
            prev = cur;
            cur = path + i;
        }
    }
    if (prev == NULL)
        return false;
    *(prev + 1) = '\0';
    return true;
}

// Simplify given path, return true if success, false if fail to simplify it
// example: /foo/bar/.././barbar/.. => /foo
bool
simplify_path(char *path)
{
    char *buff, *fname;
    bool success = false;
    size_t i, k, path_len;
    char c, cnext;

    path_len = strlen(path);
    // make sure path ends with (/)
    if (*(path + path_len - 1) != '/') {
        *(path + path_len + 1) = '\0';
        *(path + path_len) = '/';
    }
    buff = malloc(sizeof(char) * PATH_MAX);
    fname = malloc(sizeof(char) * NAME_MAX);
    memset(buff, 0, sizeof(char) * PATH_MAX);
    memset(fname, 0, sizeof(char) * NAME_MAX);
    // check if path starts from root
    if (*path != '/')
        goto done;
    // copy root path (/)
    *buff = *path;
    k = 0;
    for (i = 1; i < strlen(path); i++) {
        c = *(path + i);
        cnext = *(path + (i + 1));
        if (c == '.' && cnext == '.') {
            if (!to_dir_path(buff))
                goto done;
        }
        else if (c == '.' && cnext == '/') {
            i++;
        }
        else if (c == '/') {
            *(fname + k) = '\0';
            if (strcmp(fname, "") != 0) {
                strlcat(buff, fname,
                        strlen(buff) + strlen(fname) + 1);
                strlcat(buff, "/",
                        strlen(buff) + strlen("/") + 1);
            }
            k = 0;
            *(fname + k) = '\0';
        }
        else {
            *(fname + k) = c;
            k++;
        }
    }
    memset(path, 0, strlen(path));
    memcpy(path, buff, strlen(buff));
    success = true;

done:
    free(buff);
    free(fname);
    return success;
}

// Check if given path of directory exist
struct dir*
get_dir_by_path(const char *path)
{
    struct inode *dir_inode;
    struct dir *dir = dir_open_root();
    struct dir *return_dir = NULL;
    struct dir_entry next_dir_entry;
    off_t next_dir_ofs;
    char *fname;
    size_t i, j;
    char c;
    
    j = 0;
    fname = malloc(sizeof(char) * NAME_MAX);
    memset(fname, 0, sizeof(char) * NAME_MAX);
    for (i = 1; i < strlen(path) ; i++)
    {
        c = *(path + i);
        if (c == '/') {
            *(fname + j) = '\0';
            if (lookup(dir, fname, &next_dir_entry,
                        &next_dir_ofs)) {
                dir_inode = inode_open(next_dir_entry.inode_sector);
                dir_close(dir);
                dir = dir_open(dir_inode); 
                j = 0;
            }
            else {
                dir_close(dir);
                goto done;
            }
        }
        else {
            *(fname + j) = c;
            j++;
        }
    }
    return_dir = dir;

done:
    free(fname);
    return return_dir;
}

void
to_abs(char *path)
{
    struct thread *cur = thread_current();
    size_t cwd_len = strlen(cur->cwd);
    char *buff = malloc(sizeof(char) * PATH_MAX);

    memset(buff, 0, sizeof(char) * PATH_MAX);
    memcpy(buff, path, strlen(path));
    if (*path != '/') {
        memcpy(path, cur->cwd, strlen(cur->cwd) + 1);
        strlcat(path, buff, strlen(path) + strlen(buff) + 1);
    }
    free(buff);
}

bool
dir_chdir (const char *path)
{
    char *buff;
    struct thread *cur = thread_current();
    bool success = false;
    struct dir *dir = NULL;

    buff = malloc(sizeof(char) * PATH_MAX);
    memset(buff, 0, sizeof(char) * PATH_MAX);
    memcpy(buff, path, strlen(path));
    to_abs(buff);
    if (simplify_path(buff)
            && (dir = get_dir_by_path(buff)) != NULL) {
        success = true;
        memcpy(cur->cwd, buff, PATH_MAX);
    }
    dir_close(dir);
    free(buff);
    return success;
}

bool
dir_mkdir(const char *path)
{
    char *buff;
    char *parent_dir_path;
    char *dirname;
    struct dir *parent;
    bool success = false;
    block_sector_t dirsector;

    buff = malloc(sizeof(char) * PATH_MAX);
    parent_dir_path = malloc(sizeof(char) * PATH_MAX);
    dirname = malloc(sizeof(char) * NAME_MAX);
    memset(dirname, 0, sizeof(char) * NAME_MAX);
    memset(buff, 0, sizeof(char) * PATH_MAX);
    memset(parent_dir_path, 0, sizeof(char) * PATH_MAX);
    memcpy(buff, path, strlen(path));
    to_abs(buff);
    if (!simplify_path(buff))
        goto done;
    memcpy(parent_dir_path, buff, strlen(buff));
    if (!to_dir_path(parent_dir_path))
        goto done;
    if ((parent = get_dir_by_path(parent_dir_path)) == NULL)
        goto done;
    memcpy(dirname, buff + strlen(parent_dir_path),
            strlen(buff) - strlen(parent_dir_path) - 1);
    if (lookup(parent, dirname, NULL, NULL))
        goto done;
    if (!free_map_allocate(1, &dirsector) ||
            !dir_create(dirsector, 16))
        goto done;
    dir_add(parent, dirname, dirsector);
    success = true;

done:
    free(buff);
    free(parent_dir_path);
    dir_close(parent);
    free(dirname);
    return success;
}

bool
readdir(int fd, char *name)
{
    struct file_fd *ffd = NULL;

    get_filefd_from_fd(fd, &ffd);
    if (ffd == NULL)
        return false;
    // struct dir *dir = dir_open(ffd->f->inode);
    // if (dir == NULL)
    //     return false;

    return dir_readdir(ffd->dir, name);  
}

bool
dir_isdir(int fd)
{
    struct file_fd *ffd = NULL;
    
    get_filefd_from_fd(fd, &ffd);
    return ffd != NULL && ffd->f->inode->data.isdir;
}

int inumber (fd)
{
    struct file_fd *ffd = NULL;

    get_filefd_from_fd(fd, &ffd);
    if (ffd == NULL)
        return -1;
    return ffd->f->inode->sector;
}
