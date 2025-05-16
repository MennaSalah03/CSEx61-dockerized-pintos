#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include <stdlib.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "lib/kernel/console.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

#define MAX_ARGUMENTS 3


struct lock file_lock;
static void syscall_handler (struct intr_frame *);

struct user_file *get_file(int fd);

int getpage_ptr(const void *vaddr);
bool validate_vaddr(const void* vaddr);
bool validate_string(const void* string);
bool validate_buffer(void *buffer, unsigned size);
int args[MAX_ARGUMENTS];

void exit_handle(struct intr_frame *frame);
void exit(int status);

void write_handle(struct intr_frame *frame);
int write(int fd, void *buffer, unsigned size);

void sys_halt(void);

void create_handle(struct intr_frame *f);
bool sys_create(const char *file, unsigned initial_size);

void remove_handle(struct intr_frame *f);
bool sys_remove(const char *file);

void filesize_handle(struct intr_frame *f);
int sys_filesize(int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


/* Converts a user-space virtual address into a kernel-accessible pointer, 
Validating that the memory is properly mapped and exits the process if invalid */
int getpage_ptr(const void *vaddr) {
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr)
    exit(-1);
  
  return (int)ptr;
}

/* Checks the validity of Virtual Address 
It is unvalid if it's NULL, greater than PHYS_BASE or if the virtual address is unmapped */
bool validate_vaddr(const void* vaddr) {
  if (vaddr == NULL || vaddr >= PHYS_BASE
    || !(pagedir_get_page(thread_current()->pagedir, vaddr)))
    return false;
  return true;
}

/* Checks the validity of strings */
bool validate_string(const void* string)
{
  for (; * (char *) getpage_ptr(string) != 0; string = (char *) string + 1);
}

/* Get Arguments from the Stack */
void get_arguments(struct intr_frame *f, int *args, int num_of_args) {
  int i;
  int *ptr;

  for (i = 0; i < num_of_args; i++)
  {
    ptr = (int *) f->esp + i + 1;
    validate_vaddr((const void *) ptr);
    args[i] = *ptr;
  }
}


bool validate_buffer(void *buffer, unsigned size)
{
  unsigned i;
  char *local_buffer = (char *) buffer;
  
  for (i = 0; i < size; i++) {
    if (!validate_vaddr(local_buffer + i))
      return false;
  }
  return true;
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int sp = *(int *) f->esp;
  if (!is_user_vaddr(f->esp) || !validate_vaddr(f->esp))
  {
    exit(-1);
    return;
  }

  switch (sp)
  {
    case SYS_EXIT:
      exit_handle(f);
      break;
    case SYS_WRITE:
      write_handle(f);
      break;
    case SYS_READ:
    case SYS_FILESIZE:
      filesize_handle(f);
      break;
    case SYS_WAIT:
    case SYS_EXEC:
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_OPEN:
    case SYS_CLOSE:
    case SYS_REMOVE:
      remove_handle(f);
      break;
    case SYS_SEEK:
    case SYS_TELL:
    case SYS_CREATE:
      create_handle(f);
      break;
    default:
      printf("No system call %d\n", sp);
      exit(-1);
      break;
  }
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
  return NULL;
}


/*Handles the exit syscall and calls it*/
void exit_handle(struct intr_frame *frame)
{
  int status;
  if (!is_user_vaddr(frame->esp) || !validate_vaddr(frame->esp))
  {
    exit(-1);
    return;
  }
  if (!is_user_vaddr(frame->esp + 4) || !validate_vaddr(frame->esp + 4))
  {
    exit(-1);
    return;
  }
  status = *((int *) (frame->esp + 4));
  frame->eax = 0;
  exit(status); // Exit call
}

/* Terminates the current user program, returning status to the kernel. If the process’s
parent waits for it (see below), this is the status that will be returned. Conventionally,
a status of 0 indicates success and nonzero values indicate errors.*/
void exit(int status)
{
  struct thread *current_thread = thread_current();

  current_thread->exit_status = status;
  printf("%s exited with status %d\n", current_thread->name, status);

  //closing opened files
  struct list_elem *el;
  while (!list_empty(&current_thread->files))
  {
    el = list_begin(&current_thread->files);
    struct user_file *f = list_entry(el, struct user_file, file_elem);
    file_close(f->file);
    list_remove(&f->file_elem);
  }

  thread_exit();
}

/* Handles the write syscall */
void write_handle(struct intr_frame *frame)
{

  if (!validate_vaddr(frame->esp + 4) ||
      !validate_vaddr(frame->esp + 8) ||
      !validate_vaddr(frame->esp + 12))
   {
    exit(-1);
    return;
   }
  unsigned size = (unsigned) (*((int *) frame->esp + 12));;
  int fd = *((int *) frame->esp + 4);
  char *buffer = (char *)(*((int *) frame->esp + 8));
  if (!validate_buffer(buffer, size))
  {
    exit(-1);
    return;
  }
  frame->eax = write(fd, buffer, size);
}
/* write() writes up to count bytes from the buffer starting at buf to the
file referred to by the file descriptor fd.
*/
int write(int fd, void *buffer, unsigned size)
{
  int size_in_bytes;
  struct user_file *write_file;
  if (fd == 1)
  {
    lock_acquire(&file_lock);
    putbuf(buffer, size);
    lock_release(&file_lock);
    return size; // in bytes
  }
  write_file = get_file(fd);
  if (write_file == NULL)
  {
    return -1;
  }
}

 /* Terminates Pintos*/
void sys_halt(void)
{
  shutdown_power_off();
}

void create_handle(struct intr_frame *f)
{
  get_arguments(f, &args[0], 2);

  validate_string((const void *)args[0]);
  args[0] = getpage_ptr((const void *) args[0]);

  f->eax = sys_create((const char *)args[0], (unsigned)args[1]);
}

/* Creates a new file called file initially initial_size bytes in size
Returns true if successful, false otherwise */
bool sys_create(const char *file, unsigned initial_size)
{
  bool success;

  lock_acquire(&file_lock);
  success = filesys_create(file, initial_size);
  lock_release(&file_lock);

  return success;
}

/* Handles the remove syscall */
void remove_handle(struct intr_frame *f)
{
  get_arguments(f, &args[0], 1);

  validate_string((const void *)args[0]);
  args[0] = getpage_ptr((const void *) args[0]);

  f->eax = sys_remove((const char *)args[0]);
}


/* Deletes the file called file
Returns true if successful, false otherwise */
bool sys_remove(const char *file)
{
  bool success;

  lock_acquire(&file_lock);
  success = filesys_remove(file);
  lock_release(&file_lock);

  return success;
}

void filesize_handle(struct intr_frame *f)
{
  get_arguments(f, &args[0], 1);

  f->eax = sys_filesize(args[0]);
}

int sys_filesize(int fd)
{
  int filesize;
  
  lock_acquire(&file_lock);
  struct file *file_ptr = get_file(fd);

  if (!file_ptr) {
    lock_release(&file_lock);
    return (-1);
  }

  filesize = file_length(file_ptr);
  lock_release(&file_lock);

  return filesize;
}
