#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num = *(uint32_t *)(f->esp);
  // printf("syscall : %d\n",syscall_num);
  switch (syscall_num) {
    case SYS_HALT:
      break;
    case SYS_EXIT:
      exit(*(uint32_t *)(f->esp + 20));
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      write((int)*(uint32_t *)(f->esp+20), (void *)*(uint32_t *)(f->esp + 24), (unsigned)*((uint32_t *)(f->esp + 28)));
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
  }

  // thread_exit ();
}

void 
exit (int status) 
{
  thread_exit ();
}

int 
write (int fd, const void *buffer, unsigned size) 
{
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1; 
}