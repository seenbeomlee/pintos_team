#include "userprog/syscall.h"
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
syscall_handler (struct intr_frame *f) 
{
  int syscall_num = *(uint32_t *)(f->esp);
  // printf("syscall : %d\n",syscall_num);
  switch (syscall_num) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_user_vaddr(f->esp + 4);
      exit(*(uint32_t *)(f->esp + 4));
      break;
    case SYS_EXEC:
      check_user_vaddr(f->esp + 4);
      f->eax = exec((const char *)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_WAIT:
      check_user_vaddr(f->esp + 4);
      f->eax = wait((pid_t)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_CREATE:
      check_user_vaddr(f->esp + 4);
      check_user_vaddr(f->esp + 8);
      f->eax = create((const char *)*(uint32_t *)(f->esp + 4), (unsigned)*(uint32_t *)(f->esp + 8));
      break;
    case SYS_REMOVE:
      check_user_vaddr(f->esp + 4);
      f->eax = remove((const char*)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_OPEN:
      check_user_vaddr(f->esp + 4);
      f->eax = open((const char*)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_FILESIZE:
      check_user_vaddr(f->esp + 4);
      f->eax = filesize((int)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_READ:
      check_user_vaddr(f->esp + 20);
      check_user_vaddr(f->esp + 24);
      check_user_vaddr(f->esp + 28);
      f->eax = read((int)*(uint32_t *)(f->esp+20), (void *)*(uint32_t *)(f->esp + 24), (unsigned)*((uint32_t *)(f->esp + 28)));
      break;
    case SYS_WRITE:
      write((int)*(uint32_t *)(f->esp+20), (void *)*(uint32_t *)(f->esp + 24), (unsigned)*((uint32_t *)(f->esp + 28)));
      break;
    case SYS_SEEK:
      check_user_vaddr(f->esp + 4);
      check_user_vaddr(f->esp + 8);
      seek((int)*(uint32_t *)(f->esp + 4), (unsigned)*(uint32_t *)(f->esp + 8));
      break;
    case SYS_TELL:
      check_user_vaddr(f->esp + 4);
      f->eax = tell((int)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_CLOSE:
      check_user_vaddr(f->esp + 4);
      close((int)*(uint32_t *)(f->esp + 4));
      break;
  }
  // thread_exit ();
}

void 
halt (void) 
{
  shutdown_power_off();
}

void 
exit (int status) 
{
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_status = status;
  for (int i = 3; i < 128; i++) {
      if (thread_current()->fd[i] != NULL) {
          close(i);
      }   
  }   
  thread_exit (); 
}

pid_t 
exec (const char *cmd_line) 
{
  return process_execute(cmd_line);
}

int 
wait (pid_t pid) 
{
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) {
  return filesys_create(file, initial_size);
}

bool remove (const char *file) {
  return filesys_remove(file);
}

int open (const char *file) {
  int i;
  struct file* fp = filesys_open(file);
  if (fp == NULL) {
      return -1; 
  } else {
    for (i = 3; i < 128; i++) {
      if (thread_current()->fd[i] == NULL) {
        thread_current()->fd[i] = fp; 
        return i;
      }   
    }   
  }
  return -1; 
}

int filesize (int fd) {
  return file_length(thread_current()->fd[fd]);
}

int read (int fd, void* buffer, unsigned size) {
  int i;
  if (fd == 0) {
    for (i = 0; i < size; i ++) {
      if (((char *)buffer)[i] == '\0') {
        break;
      }   
    }   
  } else if (fd > 2) {
    return file_read(thread_current()->fd[fd], buffer, size);
  }
  return i;
}

int write (int fd, const void *buffer, unsigned size) {


  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  } else if (fd > 2) {
    return file_write(thread_current()->fd[fd], buffer, size);
  }
  return -1; 
}

void seek (int fd, unsigned position) {
  file_seek(thread_current()->fd[fd], position);
}

unsigned tell (int fd) {
  return file_tell(thread_current()->fd[fd]);
}

void close (int fd) {
  return file_close(thread_current()->fd[fd]);
}

void 
check_user_vaddr(const void *vaddr) 
{
  if (!is_user_vaddr(vaddr)) {
    exit(-1);
  }
}