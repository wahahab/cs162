#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/path.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size)
{
    // check path length
    if (strlen(path) >= PATH_MAX)
        return false;
    char *buff = malloc(sizeof(char) * PATH_MAX);
    char fname[NAME_MAX + 1];
    struct dir *dir = NULL;
    block_sector_t inode_sector = 0;
    bool success = false;

    memset(buff, 0, sizeof(char) * PATH_MAX);
    memcpy(buff, path, strlen(path));
    to_abs(buff);
    if (simplify_path(buff)) {
        path_basename(buff, fname);
        if (to_dir_path(buff)
                && (dir = get_dir_by_path(buff)) != NULL
                && free_map_allocate(1, &inode_sector)
                && inode_create(inode_sector, initial_size)
                && dir_add(dir, fname, inode_sector))
        {
            success = true;
        }
    }
    if (inode_sector != 0 && success == false)
        free_map_release(inode_sector, 1);
    dir_close(dir);
    free(buff);
    return success;
  // block_sector_t inode_sector = 0;
  // struct dir *dir = dir_open_root ();
  // bool success = (dir != NULL
  //                 && free_map_allocate (1, &inode_sector)
  //                 && inode_create (inode_sector, initial_size)
  //                 && dir_add (dir, name, inode_sector));
  // if (!success && inode_sector != 0)
  //   free_map_release (inode_sector, 1);
  // dir_close (dir);

  // return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *path)
{
    char fname[NAME_MAX];
    char *buff = malloc(sizeof(char) * PATH_MAX);
    char *dir_path = malloc(sizeof(char) * PATH_MAX);
    struct dir *dir = NULL;
    struct inode *inode = NULL;

    memset(buff, 0, sizeof(char) * PATH_MAX);
    memset(dir_path, 0, sizeof(char) * PATH_MAX);
    memset(fname, 0, NAME_MAX);
    // if it's relative path, check cwd exist or not
    if (*(path) == '.' && *(path + 1) == '.') {
        buff[0] = '.';
        to_abs(buff);
        if (simplify_path(buff) && get_dir_by_path(buff) == NULL)
            goto done;
    }
    else if (*path == '\0') {
        goto done;
    }
    memset(buff, 0, sizeof(char) * PATH_MAX);
    memcpy(buff, path, strlen(path));
    to_abs(buff);
    if (simplify_path(buff))
    {
        memcpy(dir_path, buff, strlen(buff));
        if (strcmp(buff, "/") == 0) {
            dir = dir_open_root();
            inode = dir_get_inode(dir);
        }
        else if (to_dir_path(dir_path)
                && (dir = get_dir_by_path(dir_path)) != NULL) {
            path_basename(buff, fname);
            dir_lookup(dir, fname, &inode);
        }
    }

done:
    dir_close(dir);
    free(dir_path);
    free(buff);
    return file_open(inode);
  // struct dir *dir = dir_open_root ();
  // struct inode *inode = NULL;

  // if (dir != NULL)
  //   dir_lookup (dir, name, &inode);
  // dir_close (dir);

  // return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *path)
{
    char fname[NAME_MAX];
    char *buff = malloc(sizeof(char) * PATH_MAX);
    char *dir_path = malloc(sizeof(char) * PATH_MAX);
    struct dir *dir = NULL;
    bool success = false;

    memset(buff, 0, sizeof(char) * PATH_MAX);
    memset(dir_path, 0, sizeof(char) * PATH_MAX);
    memset(fname, 0, NAME_MAX);
    memcpy(buff, path, strlen(path));
    to_abs(buff);
    if (simplify_path(buff))
    {
        memcpy(dir_path, buff, strlen(buff));
        if (to_dir_path(dir_path)
                && (dir = get_dir_by_path(dir_path)) != NULL) {
            path_basename(buff, fname);
            success = dir_remove(dir, fname);
        }
    }
    dir_close(dir);
    free(dir_path);
    free(buff);
    return success;

  // struct dir *dir = dir_open_root ();
  // bool success = dir != NULL && dir_remove (dir, name);
  // dir_close (dir);

  // return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
