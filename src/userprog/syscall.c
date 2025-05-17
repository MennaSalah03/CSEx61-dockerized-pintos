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

#define MAX_ARGUMENTS 32


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


void open_handle(struct intr_frame *f);
int sys_open(const char *file);


void seek_handle(struct intr_frame *f);
void seek(int fd, unsigned position);

void close_handle(struct intr_frame *f);
void close(int fd);

unsigned tell(int fd);
void tell_handle(struct intr_frame *f);
void read_handle(struct intr_frame *f);
int read(int fd, void *buffer, unsigned size);

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
    case SYS_WAIT:
      printf("Not implemented\n");
      exit(-1);
      break;
    case SYS_EXEC:
      printf("Not implemented\n");
      exit(-1);
      break;
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_OPEN:
      open_handle(f);
      break;
    case SYS_CLOSE:
      close_handle(f);
      exit(-1);
      break;
    case SYS_REMOVE:
      remove_handle(f);
    case SYS_SEEK:
      seek_handle(f);
      break;
    case SYS_TELL:
      printf("Not implemented\n");
      exit(-1);
      break;
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
  lock_acquire(&file_lock);
  size_in_bytes = file_write(write_file->file, buffer, size);
  lock_release(&file_lock);
  return size_in_bytes;
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
  lock_acquire(&file_lock);
  struct file *file_ptr = get_file(fd);

  if (!file_ptr) {
    lock_release(&file_lock);
    return (-1);
  }

  int filesize = file_length(file_ptr);
  lock_release(&file_lock);

  return filesize;
}


/* Handles the SYS_OPEN system call */
void open_handle(struct intr_frame *f)
{
  // Get the argument (pointer to file name)
  get_arguments(f, &args[0], 1);

  // Validate the string (file name)
  validate_string((const void *)args[0]);

  // Convert to kernel virtual address
  args[0] = getpage_ptr((const void *) args[0]);

  // Call the sys_open function and return result in eax
  f->eax = sys_open((const char *)args[0]);
}

/* Opens the file and returns a file descriptor */
int sys_open(const char *file) {
  // if (file == NULL)
  //   return -1;

  // lock_acquire(&file_lock);

  // struct file *opened_file = filesys_open(file);

  // if (opened_file == NULL) {
  //   lock_release(&file_lock);
  //   return -1;
  // }

  // struct thread *curr = thread_current();

  // // Allocate memory for user_file struct
  // struct user_file *uf = malloc(sizeof(struct user_file));
  // if (!uf) {
  //   file_close(opened_file);
  //   lock_release(&file_lock);
  //   return -1;
  // }

  // uf->file = opened_file;
  // uf->fd = curr->fd_tracker++;

  // // Add to current thread's file list
  // list_push_back(&curr->files, &uf->file_elem);

  // lock_release(&file_lock);

  // return uf->fd;
}

void seek_handle(struct intr_frame *f) {
  get_arguments(f, &args[0], 2);
  int fd = args[0];
  unsigned position = args[1];

  seek(fd, position);
}
void seek(int fd, unsigned position) {
  struct user_file *uf = get_file(fd);
  if (uf == NULL)
    return;

  lock_acquire(&file_lock);
  file_seek(uf->file, position);
  lock_release(&file_lock);
}

void close_handle(struct intr_frame *f) {
  get_arguments(f, &args[0], 1);
  close(args[0]);
}
void close(int fd) {
  struct user_file *uf = get_file(fd);
  if (uf == NULL)
    return;

  lock_acquire(&file_lock);
  file_close(uf->file);                     // Close the file
  list_remove(&uf->file_elem);             // Remove from thread's file list
  free(uf);                                 // Free the memory
  lock_release(&file_lock);
}
void read_handle(struct intr_frame *frame)
{
  int args[3];
  get_arguments(frame, args, 3);
  int fd = args[0];;
  void *buffer = (void *)args[1];
  unsigned size = (unsigned) args[2];

  if (!is_user_vaddr(buffer)  || !is_user_vaddr((uint8_t *)buffer + size - 1))
    exit(-1);
  validate_buffer(buffer, size);
  frame->eax = read(fd, buffer, size);

}
/*Reads size bytes from the file open as fd into buffer. Returns the number of bytes
actually read (0 at end of file), or -1 if the file could not be read (due to a condition
other than end of file). Fd 0 reads from the keyboard using input_getc().*/
/* Read system call handler */
int read(int fd, void *buffer, unsigned size) {
    //standard input (keyboard)
    if (fd == 0) {
        unsigned i;
        uint8_t* buf = (uint8_t*)buffer;
        for (i = 0; i < size; i++) {
            buf[i] = input_getc();
            if (buf[i] == '\n')
                break;
        }
        return i;
    }
    
    // Get file from file
    struct user_file *uf = get_file(fd);
    if (uf == NULL)
        return -1;
        
    // Read from file
    lock_acquire(&file_lock);
    int bytes_read = file_read(uf->file, buffer, size);
    lock_release(&file_lock);
    
    return bytes_read;
}

void tell_handle(struct intr_frame *f) {
  int args[1];
  get_arguments(f, args, 1);
  int fd = args[0];
  f->eax = tell(fd);
}
/*Returns the position of the next byte to be read or written in open  le fd, expressed
in bytes from the beginning of the file.*/
unsigned tell(int fd) {
  struct user_file *uf = get_file(fd);
  if (uf == NULL || uf->file == NULL)
    return 0;

  lock_acquire(&file_lock);
  unsigned next_byte_pos = file_tell(uf->file);
  lock_release(&file_lock);

  return next_byte_pos;
}