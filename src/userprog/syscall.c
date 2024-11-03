#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
void check_sp (void *uaddr);

struct lock file_lock;


void
syscall_init (void)
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_sp_onebyone(void* addr, int size){
  for (int i = 0; i < size; i++)
    {
      check_sp((void *)addr+i);
    }
}

static void
syscall_handler (struct intr_frame *f)
{
  //printf ("system call!\n");

  void *sp = f->esp;
  check_sp(sp); check_sp(sp + 4); check_sp(sp + 8); check_sp(sp + 12);
  switch(*(uint32_t *)sp){
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_sp_onebyone(sp + 4, sizeof(uint32_t));
      exit(*(uint32_t *)(sp + 4));
      break;
    case SYS_EXEC:
      check_sp_onebyone(sp + 4, sizeof(const char *));
      f->eax = exec((const char *)*(uint32_t *)(sp+4));
      break;
    case SYS_WAIT:
      check_sp_onebyone(sp + 4, sizeof(pid_t));
      f->eax = wait(*(pid_t*)(sp+4));
      break;
    case SYS_CREATE:
      check_sp_onebyone(sp + 4, sizeof(const char*));
      check_sp_onebyone(sp + 8, sizeof(unsigned));
      f->eax = create((const char *)*(uint32_t *)(sp + 4), (const char *)*(uint32_t *)(sp + 8));
      break;
    case SYS_REMOVE:
      check_sp_onebyone(sp + 4, sizeof(const char*));
      f->eax = remove((const char *)*(uint32_t *)(sp + 4));
      break;
    case SYS_OPEN:
      check_sp_onebyone(sp + 4, sizeof(const char*));
      f->eax = open((const char *)*(uint32_t *)(sp + 4));
      break;
    case SYS_FILESIZE:
      check_sp_onebyone(sp + 4, sizeof(int));
      f->eax = filesize((int)*(uint32_t *)(sp + 4));
      break;
    case SYS_READ:
      check_sp_onebyone(sp + 4, sizeof(int));
      check_sp_onebyone(sp + 8, sizeof(void*));
      check_sp_onebyone(sp + 12, sizeof(unsigned));
      f->eax = read((int)*(uint32_t *)(sp + 4), (void *)*(uint32_t *)(sp + 8), (unsigned)*((uint32_t *)(sp + 12)));
      break;
    case SYS_WRITE:
      check_sp_onebyone(sp + 4, sizeof(int));
      check_sp_onebyone(sp + 8, sizeof(void*));
      check_sp_onebyone(sp + 12, sizeof(unsigned));
      f->eax = write((int)*(uint32_t *)(sp + 4), (void *)*(uint32_t *)(sp + 8), (unsigned)*((uint32_t *)(sp + 12)));
      break;
    case SYS_SEEK:
      check_sp_onebyone(sp + 4, sizeof(int));
      check_sp_onebyone(sp + 8, sizeof(unsigned));
      seek((int)*(uint32_t *)(sp + 4), (unsigned)*((uint32_t *)(sp + 8)));
      break;
    case SYS_TELL:
      check_sp_onebyone(sp + 4, sizeof(int));
      f->eax = tell((int)*(uint32_t *)(sp + 4));
      break;
    case SYS_CLOSE:
      check_sp_onebyone(sp + 4, sizeof(int));
      close((int)*(uint32_t *)(sp + 4));
      break;
    default:
      exit(-1);
  }
  //thread_exit ();
}

void halt() {
  shutdown_power_off();
}

void exit(int status) {
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_code = status;
  thread_exit();
}

pid_t exec (const char *file) {
    //printf("exec call\n");
    check_sp_onebyone(file, sizeof(file));
    return process_execute(file);
}

int wait (pid_t pid) {
    //printf("wait call\n");
    return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
 //printf("create call\n");
 check_sp_onebyone(file, sizeof(file));
    if(file == NULL)
        exit(-1);
    check_sp(file);
    return filesys_create(file, initial_size);
}

bool remove (const char *file){
  //printf("remove call\n");
  check_sp_onebyone(file, sizeof(file));
    if(file == NULL)
        exit(-1);
    return filesys_remove(file);
}

int open (const char *file){
   //printf("open call\n");
   check_sp_onebyone(file, sizeof(file));
   if(file == NULL)
      exit(-1);
    check_sp(file);
    lock_acquire(&file_lock);
    struct file *open_result = filesys_open(file);
    int ret;
    if(open_result == NULL){
        lock_release(&file_lock);
        return -1;
    }
    else{
       for(int i = 3; i<128; i++){
          if(thread_current()->file_descriptor[i] == NULL){
            if(strcmp(thread_current()->name, file) == 0){
              file_deny_write(open_result);
            }
             thread_current()->file_descriptor[i] = open_result;
             ret = i;
             break;
       }
    }
    lock_release(&file_lock);
    return ret;
  }
}

int filesize (int fd){
    //printf("filesize call\n");
    struct file *target = thread_current()->file_descriptor[fd];
    if(target == NULL)
        exit(-1);
    else
        return file_length (target);
}

int read (int fd, void *buffer, unsigned size){
//  printf("read call\n");
  check_sp_onebyone(buffer, sizeof(buffer));
    int ret = -1;
    check_sp(buffer);
    lock_acquire(&file_lock);
    if(fd == 0){
        for(int i = 0; i<size; i++){
            if(input_getc() == '\0'){
                ret = i;
                break;
            }
        }
    }
    else if(fd > 2 && fd < 128){
        if(thread_current()->file_descriptor[fd] == NULL){
            lock_release(&file_lock);
            exit(-1);
        }
        ret = file_read(thread_current()->file_descriptor[fd], buffer, size);
    }
    else{
      lock_release(&file_lock);
      exit(-1);
    }

    lock_release(&file_lock);
    return ret;
}

int write (int fd, const void *buffer, unsigned size){
  //  printf("write call\n");
    check_sp_onebyone(buffer, sizeof(buffer));
    check_sp(buffer+size);
    int ret = -1;
    lock_acquire(&file_lock);
    if(fd == 1) {
        putbuf(buffer, size);
        ret = size;
    }
    else if(fd > 2 && fd < 128){
        if(thread_current()->file_descriptor[fd] == NULL){
            lock_release(&file_lock);
            exit(-1);
        }
        if(thread_current()->file_descriptor[fd]->deny_write)
            file_deny_write(thread_current()->file_descriptor[fd]);
        ret = file_write(thread_current()->file_descriptor[fd], buffer, size);
    }
    else{
      lock_release(&file_lock);
      exit(-1);
    }
    lock_release(&file_lock);
    return ret;
}

void seek (int fd, unsigned position){
  //  printf("seek call\n");
    struct file *target = thread_current()->file_descriptor[fd];
    if(target == NULL)
        exit(-1);
    else
        return file_seek (target, position);
}

unsigned tell (int fd){
    //printf("tell call\n");
    struct file *target = thread_current()->file_descriptor[fd];
    if(target == NULL)
        exit(-1);
    else
        return file_tell (target);
}

void close (int fd){
  //  printf("close call\n");
    if (fd < 0 || fd >= 128)
      exit(-1);
    struct file *target = thread_current()->file_descriptor[fd];
    if(target == NULL)
        exit(-1);
    else{
        file_close(target);
        thread_current()->file_descriptor[fd] = NULL;
    }
}

void check_sp (void *uaddr){
  if (uaddr == NULL || !((unsigned int)uaddr < PHYS_BASE) || pagedir_get_page(thread_current()->pagedir, uaddr) == NULL){
    exit(-1);
  }
}
