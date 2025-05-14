#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>

void syscall_init (void);
bool validate_vaddr(const void* vaddr);
bool validate_string(const void* string);
void sys_halt(void);
bool sys_create(struct intr_frame *f);

struct lock file_lock;
#endif /* userprog/syscall.h */
