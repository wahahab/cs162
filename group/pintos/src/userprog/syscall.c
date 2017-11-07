#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
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
        printf("%s: exit(%d)\n", (char*) &thread_current ()->name, status);
        thread_exit();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  int return_status = 0;
  int callno;
  bool success;
  struct file *opened_file;
  struct file_fd *opened_file_fd;
  struct thread *cur = thread_current();

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
    case SYS_WRITE:
        if (args[1] == 1)
            putbuf((char*)args[2], args[3]);
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
    case SYS_OPEN:
        if (args[1] == NULL) {
            f->eax = -1;
            pexit(-1);
        }
        f->eax = process_open_file(args[1]);
        break;
    case SYS_READ:
        f->eax = process_read_file(args);
        break;
    case SYS_FILESIZE:
        f->eax = process_file_size(args[1]);
        break;
    case SYS_EXEC:
        f->eax = process_execute(args[1]);
  }
}
