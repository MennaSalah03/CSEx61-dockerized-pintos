#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "lib/kernel/console.h"
#include "userprog/pagedir.h"

#define MAX_FILENAME_LEN 256

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *sp = *(int *) f->esp;
  if (sp == NULL || sp >= PHYS_BASE)
    exit(-1);

  switch (*sp)
  {
    case SYS_EXIT:
      return exit_handle(f);
      break;
    case SYS_WRITE:
      return write_handle(f);
      break;
    case SYS_READ:
    case SYS_FILESIZE:
    case SYS_WAIT:
    case SYS_EXEC:
    case SYS_HALT:
      return sys_halt();
      break;
    case SYS_OPEN:
    case SYS_CLOSE:
    case SYS_REMOVE:
    case SYS_SEEK:
    case SYS_TELL:
    case SYS_CREATE:
      return create_handle(f);
      break;
    default:
      printf("No system call %d\n", *sp);
      break;
  }
  printf ("system call!\n");
  thread_exit ();
}

struct user_file *get_file(int fd)
{
  struct list *files = &(thread_current())->files;
  struct list_elem *file = list_begin(files);
  while(file != list_end(files))
  {
    struct user_file *f = list_entry(file, struct user_file, file_elem);
    if (f->fd == fd)
      return f;
    
    file = list_next(file);
  }
}


/*Handles the exit syscall and calls it*/
void exit_handle(struct intr_frame *frame)
{
  int status = *((int *) frame->esp + 1);
  if (!is_user_vaddr(status))
  {
    frame->eax = -1;
    return exit(-1);
  }
  frame->eax = status;
  exit(status); // Exit call
}

/* Terminates the current user program, returning status to the kernel. If the process’s
parent waits for it (see below), this is the status that will be returned. Conventionally,
a status of 0 indicates success and nonzero values indicate errors.*/
void exit(int status)
{
  struct thread *current_thread = thread_current();
  char *save_ptr;
  char *name = current_thread->name;
  char *executable = strtok_r(name, " ", &save_ptr);

  current_thread->exit_status = status;
  prinf("%s exited with status %d\n", current_thread->name, status);
  thread_exit();
}

/* Handles the write syscall */
void write_handle(struct intr_frame *frame)
{
  unsigned size;
  int fd = *((int *) frame->esp + 1);
  char *buffer = (char *)(*((int *) frame->esp + 2));
  if (fd == 0 || !is_user_vaddr(buffer))
    exit(-1); 

  size = (unsigned) (*((int *) frame->esp + 3));
  frame->eax = write(fd, buffer, size);
}
/* write() writes up to count bytes from the buffer starting at buf to the
file referred to by the file descriptor fd.
*/
int write(int fd, void *buffer, unsigned size)
{
  struct file *write_file;
  if (fd == 1)
  {
    lock_acquire(&file_lock);
    putbuf(buffer, size);
    lock_release(&file_lock);
    return size;
  }
  write_file = get_file(fd);
  if (write_file == NULL)
  {
    return -1;
  }
}

/* Checks the validity of Virtual Address 
It is unvalid if it's NULL, greater than PHYS_BASE or if the virtual address is unmapped */
bool validate_vaddr(const void* vaddr) {
  if (vaddr == NULL || vaddr >= PHYS_BASE || !(pagedir_get_page(thread_current()->pagedir, vaddr)))
    return false;
}

/* Checks the validity of strings like filenames
Returns False f filename is too long or not null-terminated */
bool validate_string(const void* filename) {
  if (strnlen(filename, MAX_FILENAME_LEN) == MAX_FILENAME_LEN)
    return false;
}


 /* Terminates Pintos*/
void sys_halt(void)
{
  shutdown_power_off();
}

bool sys_create(struct intr_frame *f) {

}
