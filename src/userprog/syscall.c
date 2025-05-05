#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *sp = (int *) f->esp;
  if (sp == NULL || sp >= PHYS_BASE)
    exit(-1);
    

  int syscall_num = *sp;
  switch (syscall_num)
  {
  case SYS_EXIT:
    my_exit(f);
    break;
  case SYS_WRITE:
    f->eax = my_write(f);
    break;
  case SYS_READ:
  case SYS_FILESIZE:
  case SYS_WAIT:
  case SYS_EXEC:
  case SYS_HALT:
  case SYS_OPEN:
  case SYS_CLOSE:
  case SYS_REMOVE:
  case SYS_SEEK:
  case SYS_TELL:
  case SYS_CREATE:

  default:
    printf("No system call %d\n", *sp);
    break;
  }
  printf ("system call!\n");
  thread_exit ();
}
/* Terminates the current user program, returning status to the kernel. If the process’s
parent waits for it (see below), this is the status that will be returned. Conventionally,
a status of 0 indicates success and nonzero values indicate errors.*/
void my_exit(int exit_status)
{
  struct thread *curr = thread_current();
  prinf("%s exited with status %d\n", curr->name, exit_status);
  thread_exit();
}
