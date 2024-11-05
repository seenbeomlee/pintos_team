#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
void argument_passing(void **esp, char** file_name, int argc);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
struct nameAndsem
{
  char *name;
  struct semaphore sem;
  bool success;
};

tid_t
process_execute (const char *file_name)
{
  char *fn_copy, *ptr;
  char* loading_name;
  tid_t tid;
  struct nameAndsem *ns = palloc_get_page(0);
  if (ns == NULL){
    return TID_ERROR;
  }
  sema_init(&ns->sem, 0);
  ns->success = false;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL){
    palloc_free_page(ns);
    return TID_ERROR;
  }
  loading_name = palloc_get_page (0);
  if (loading_name == NULL){
    palloc_free_page(fn_copy);
    palloc_free_page(ns);
    return TID_ERROR;
  }

  strlcpy (fn_copy, file_name, PGSIZE);
  strlcpy (loading_name, file_name, PGSIZE);
  ns->name = fn_copy;
  loading_name = strtok_r(loading_name, " ", &ptr);
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (loading_name, PRI_DEFAULT, start_process, ns);
  sema_down(&ns->sem); // 로딩이 완료될 때까지 기다리기
  bool success = ns->success;

  if (tid == TID_ERROR || !success){
    palloc_free_page (fn_copy);
    palloc_free_page(loading_name);
    palloc_free_page (ns);
    return TID_ERROR;
  }
  palloc_free_page (fn_copy);
  palloc_free_page(loading_name);
  palloc_free_page (ns);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *sem)
{
  struct nameAndsem* ns = sem;
  char *file_name = ns->name;
  struct intr_frame if_;
  bool success;
  int i;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  char *ptr;
  char* copy = palloc_get_page (0);
  int argc = 0;
  strlcpy (copy, file_name, PGSIZE);

  // 인자 개수 세기
  for (char* token = strtok_r(copy, " ", &ptr); token != NULL; token = strtok_r(NULL, " ", &ptr)){
    argc++;
  }
  strlcpy (copy, file_name, PGSIZE);

  // 인자 저장
  i = 0;
  char** file_name_copy = (char **)malloc(sizeof(char *) * argc);
  for (char* token = strtok_r(copy, " ", &ptr); token != NULL; token = strtok_r(NULL, " ", &ptr)){
    file_name_copy[i] = token;
    i++;
  }

  success = load(file_name_copy[0],  &if_.eip, &if_.esp); // 로딩 시도

  ns->success = success;
  sema_up(&ns->sem); // 로딩 완료

  if (success){
    argument_passing(&if_.esp, file_name_copy, argc);
  }

  /* If load failed, quit. */
  free(file_name_copy);
  palloc_free_page(copy);
  if (!success){
    exit(-1);
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */

int
process_wait (tid_t child_tid UNUSED)
{
  struct thread* cur = thread_current(); // 현재 실행 중인 스레드(부모 프로세스)
  struct thread* t; // 특정 자식 프로세스 스레드를 가리키기 위한 포인터
  struct list_elem* child; // 현재 스레드의 자식 스레드 리스트를 순회하는 데 사용되는 리스트 요소 포인터
  struct lock loop_lock; // 자식 스레드를 검색하고 기다리는 동안 사용되는 락

  lock_init(&loop_lock);
  int ret; // 자식 프로세스의 종료 코드

  if(list_empty(&(cur->child_threads))){
    return -1;
  } // 자식프로세스 없으면 대기안해도됨

  // 자식 프로세스 iteration
  for (child = list_front(&(cur->child_threads)); child != list_end(&(cur->child_threads)); child=list_next(child)){
    t = list_entry(child, struct thread, child_elem);
    if(t->tid == child_tid && !t->waiting){ // 현재 자식 스레드의 ID가 요청된 자식 프로세스 ID(child_tid)와 같고, 해당 스레드가 대기 상태가 아니라면
      lock_acquire(&loop_lock);
      if (!t->waiting) t->waiting = true; // lock_acquire 전에 확인한 조건이니 한번 더 확인하는게 맞음
      else{
        lock_release(&loop_lock);
        return -1;
      }
      lock_release(&loop_lock);
      sema_down(&t->child_check_sem);
      ret = t->exit_code;
      sema_up(&t->loading_sem);
      return ret;
    }
  }
  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  for (int i = 3; i < 128; i++){
    file_close(cur->file_descriptor[i]);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  sema_up(&cur->child_check_sem);
  sema_down(&cur->loading_sem);
  list_remove(&cur->child_elem);
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current (); // 현재 실행중인 스레드
  struct Elf32_Ehdr ehdr; // ELF 파일 헤더를 저장하는 구조체
  struct file *file = NULL; // 열려 있는 파일을 가리키는 포인터
  off_t file_ofs; // 파일에서 읽기 시작할 위치
  bool success = false; //로드 성공 여부
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create (); // 현재 스레드에 사용할 페이지 디렉터리를 생성
  if (t->pagedir == NULL) goto done; // 생성 실패시 정리
  process_activate (); // 스레드의 페이지 디렉터리를 활성화하여 새로 설정된 페이지 디렉터리가 현재 프로세스의 주소 공간으로 사용되도록 함

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr // 파일의 크기가 ELF 헤더 크기와 일치하는지 확인
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7) // ELF 매직 넘버와 일치하는지 확인
      || ehdr.e_type != 2 // 실행 파일인지 확인
      || ehdr.e_machine != 3 // x86 아키텍처인지 확인
      || ehdr.e_version != 1 // ELF 버전이 1인지 확인
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr) // 프로그램 헤더의 크기가 맞는지 확인
      || ehdr.e_phnum > 1024) // 프로그램 헤더의 개수가 적절한 범위 내에 있는지 확인
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff; // 프로그램 헤더 테이블의 시작 오프셋을 설정
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      // 파일 오프셋이 파일 크기보다 크거나 0보다 작으면 잘못된 오프셋이므로 goto done으로 이동
      if (file_ofs < 0 || file_ofs > file_length (file)) 
        goto done;
      
      file_seek (file, file_ofs); // 파일 포인터를 현재 프로그램 헤더가 있는 위치로 이동

      // 현재 프로그램 헤더를 읽어와 phdr에 저장
      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr) goto done;

      file_ofs += sizeof phdr; // 다음 프로그램 헤더로 이동하기 위해 file_ofs에 sizeof phdr만큼을 더함

      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break; // p_type이 위 케이스 중 하나라면 해당 세그먼트를 무시하고 다음 헤더로 넘어감
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done; // p_type이 위 케이스 중 하나라면 잘못된 타입의 세그먼트이므로 로드를 중단하고 goto done으로 이동
        case PT_LOAD:
        // p_type이 PT_LOAD인 경우(메모리에 로드할 세그먼트), validate_segment 함수로 유효성을 검사하고, 유효할 경우에만 로드 절차를 진행
          if (validate_segment (&phdr, file)) {
            bool writable = (phdr.p_flags & PF_W) != 0; // 세그먼트가 쓰기 가능한지 확인
            uint32_t file_page = phdr.p_offset & ~PGMASK; // 파일 페이지 정렬된 주소를 계산
            uint32_t mem_page = phdr.p_vaddr & ~PGMASK; // 메모리의 페이지 정렬된 주소를 계산
            uint32_t page_offset = phdr.p_vaddr & PGMASK; // 페이지 내 오프셋을 계산
            uint32_t read_bytes, zero_bytes;
            if (phdr.p_filesz > 0)
              {
                /* Normal segment.
                    Read initial part from disk and zero the rest. */
                read_bytes = page_offset + phdr.p_filesz;
                zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                              - read_bytes);
              }
            else
              {
                /* Entirely zero.
                    Don't read anything from disk. */
                read_bytes = 0;
                zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
              }
            if (!load_segment (file, file_page, (void *) mem_page,
                                read_bytes, zero_bytes, writable))
              goto done;
          }
          else
            goto done; 
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

void argument_passing(void **esp, char** file_name, int argc){

  void *addr[argc]; // 각 인자의 주소를 저장할 배열 addr을 생성. 배열 크기는 인자 개수 argc

  // 인자를 역순으로 스택에 복사
  for (int i = argc - 1; i >= 0; i--)
  {
     *esp -= strlen(file_name[i]) + 1; // 문자열 끝 \0 처리를 위해 +1
     memcpy(*esp, file_name[i], strlen(file_name[i]) + 1);
     addr[i] = *esp;
  }

  // 스택 포인터가 4바이트 배수가 될 때까지 1바이트씩 감소시키고, 정렬을 위해 추가된 바이트에 0을 채움
  while ((PHYS_BASE - *esp) % 4 != 0)
  {
    *esp -= 1;
    *((uint8_t *)*esp) = 0;
  }

  // 인자 리스트의 끝을 표시하기 위해 NULL 삽입
  *esp -= 4;
  *((uint32_t *)*esp) = 0;


  // 인자 주소 넣기
  for (int i = argc - 1 ; i >= 0; i--){
    *esp -= 4;
    *((void**)*esp) = addr[i];
  }

  // 인자 주소 리스트의 끝을 표시하기 위해 NULL 삽입
  *esp -= 4;
  *((void**)*esp) = *esp + 4;

  // argc 넣기
  *esp -= 4;
  *((int*)*esp) = argc;

  // 리턴 주소를 0으로 설정하여 스택의 최상단에 삽입. 이는 main 함수 종료 후 돌아갈 주소가 없음을 나타냄
  *esp -= 4;
  *((int*)*esp) = 0;

  //uintptr_t ofs = (uintptr_t)*esp;
  //int byte_size = PHYS_BASE - ofs;
  //hex_dump(ofs, *esp,byte_size, true);
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
