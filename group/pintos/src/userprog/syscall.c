#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
bool isvalid_address(void *p);
void pexit(int status);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool isvalid_address(void *p) {
    return (p + 4) < PHYS_BASE;
}

void pexit(int status) {
    struct thread *cur = thread_current();

        printf("%s: exit(%d)\n",
                (char*) cur->name, status);
        cur->wait_ctx->exit_status = status;
        sema_up(&cur->wait_ctx->finish_sema);
        thread_exit();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  int return_status = 0;
  int callno;
  bool success;
  int result;
  tid_t tid;

  if (!isvalid_address(args)) {
    callno = SYS_EXIT;
    return_status = -1;
  }
  else
      callno = args[0];
  switch(callno) {
    case SYS_EXIT:
        return_status = ((return_status == 0)? args[1]: -1);
        f->eax = return_status;
        pexit(return_status);
        break;
    case SYS_HALT:
        break;
    case SYS_PRACTICE:
        f->eax = args[1] + 1;
        break;
    case SYS_CREATE:
        if (args[1] == NULL || !isvalid_address(args[1]))
        {
            f->eax = -1;
            pexit(-1);
        }
        else {
            success = filesys_create(args[1], args[2]);
            f->eax = success;
        }
        break;
    case SYS_REMOVE:
        f->eax = filesys_remove(args[1]);
        break;
    case SYS_CLOSE:
        process_close_file(args[1]);
        break;
    case SYS_OPEN:
        if (args[1] == NULL) {
            f->eax = -1;
            pexit(-1);
        }
        f->eax = process_open_file(args[1]);
        break;
    case SYS_TELL:
        f->eax = process_tell_file(args[1]);
        break;
    case SYS_SEEK:
        process_seek_file(args);       
        break;
    case SYS_READ:
        if (!isvalid_address(args[2])) {
            f->eax = -1;
            pexit(-1);
        }
        f->eax = process_read_file(args);
        break;
    case SYS_WRITE:
        if (!isvalid_address(args[2])) {
            f->eax = -1;
            pexit(-1);
        }
        if (args[1] == 1)
            putbuf((char*)args[2], args[3]);
        else
            f->eax = process_write_file(args);
        break;
    case SYS_FILESIZE:
        f->eax = process_file_size(args[1]);
        break;
    case SYS_EXEC:
        tid = process_execute(args[1]);
        if (tid == TID_ERROR)
            f->eax = -1;
        else
            f->eax = tid;
        break;
    case SYS_WAIT:
        if (!isvalid_address(args[1])) {
            f->eax = -1;
            pexit(-1);
        }
        result = process_wait(args[1]);
        if (result == -2) {
            f->eax = -1;
            pexit(-1);
        }
        else if (result == -1) {
            f->eax = -1;
        }
        else
            f->eax = result;
        break;
    // project 4
    case SYS_INUMBER:
        f->eax = inumber(args[1]);
        break;
    case SYS_ISDIR:
        f->eax = dir_isdir(args[1]);
        break;
    case SYS_CHDIR:
        f->eax = dir_chdir(args[1]);
        break;
    case SYS_MKDIR:
        f->eax = dir_mkdir(args[1]);
        break;
    case SYS_READDIR:
        f->eax = readdir(args[1], args[2]);
        break;
    case SYS_TEST_SIMPATH:
        f->eax = simplify_path(args[1]);
        break;
  }
}
