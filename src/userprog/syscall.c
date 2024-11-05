#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
void check_sp (void *uaddr);

struct lock file_lock;

/* syscall_init
 * 파일 락 초기화: file_lock을 초기화하여 파일 시스템에 대한 접근을 제어하고,
 * 여러 스레드가 동시에 파일을 수정할 때 발생할 수 있는 경쟁 상태를 방지
 * 시스템 콜 인터럽트 등록: int 0x30 인터럽트를 등록하여 사용자 프로그램에서 시스템 콜이 발생할 때 syscall_handler가 호출되도록 설정
 */
void
syscall_init (void)
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// 각 바이트의 주소가 유효한 사용자 메모리인지 검사
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
  printf("%s: exit(%d)\n", thread_name(), status); // 종료 메세지 출력
  thread_current()->exit_code = status; // 현재 스레드(프로세스)의 exit_code에 종료 상태 코드를 저장
  thread_exit(); // 자원 해제 및 스레드 스케줄러에 제어 넘기기
}

pid_t exec (const char *file) {
    //printf("exec call\n");
    check_sp_onebyone(file, sizeof(file)); // file이 가리키는 주소가 유효한 사용자 메모리 영역인지 검사
    return process_execute(file); // 지정된 파일을 실행하는 새 프로세스를 생성하고, 그 프로세스의 ID를 반환
}

int wait (pid_t pid) {
    //printf("wait call\n");
    return process_wait(pid); // 특정 자식 프로세스의 종료를 기다리고, 해당 프로세스의 종료 상태 코드를 int 타입으로 반환
}

bool create (const char *file, unsigned initial_size){
 //printf("create call\n");
 check_sp_onebyone(file, sizeof(file)); // file이 가리키는 주소가 유효한 사용자 메모리인지 검사
    if(file == NULL) // file이 NULL인지 확인. 만약 file이 NULL이라면 유효하지 않은 파일 이름이므로 오류를 처리하고 프로세스를 종료
        exit(-1);
    check_sp(file); // file이 가리키는 주소가 유효한 사용자 메모리인지 추가로 확인
    return filesys_create(file, initial_size); // 파일 시스템에서 새로운 파일을 생성. 성공 여부 반환.
}

bool remove (const char *file){
  //printf("remove call\n");
  check_sp_onebyone(file, sizeof(file)); // 파일 이름이 유효한 사용자 메모리 주소인지 확인
    if(file == NULL)
        exit(-1);
    return filesys_remove(file); // 파일 시스템에서 지정된 파일을 삭제. 성공 여부 반환
}

int open (const char *file){
   //printf("open call\n");
   check_sp_onebyone(file, sizeof(file)); // 파일 이름이 유효한 사용자 메모리 주소인지 확인
   if(file == NULL) // file이 NULL인지 확인. 만약 file이 NULL이라면 유효하지 않은 파일 이름이므로 오류를 처리하고 프로세스를 종료
      exit(-1);
    check_sp(file); // file이 가리키는 주소가 유효한 사용자 메모리인지 추가로 확인
    lock_acquire(&file_lock); // file_lock을 획득하여 파일 시스템에 대한 접근을 제어
    struct file *open_result = filesys_open(file); // 파일 시스템에서 file 이름의 파일을 열기
    int ret; // 반환할 파일 디스크립터를 저장할 변수
    // 파일을 여는 데 실패하여 open_result가 NULL인 경우 file_lock을 해제하고 -1을 반환하여 오류를 알림
    if(open_result == NULL){
        lock_release(&file_lock);
        return -1;
    }
    // 파일을 여는 데 성공한 경우, 현재 스레드의 파일 디스크립터 배열에서 비어 있는 인덱스를 찾기
    else{
       for(int i = 3; i<128; i++){ // 0, 1, 2는 표준 입력, 표준 출력, 표준 오류로 예약되어 있어 3부터 사용
          if(thread_current()->file_descriptor[i] == NULL){ // 현재 스레드의 file_descriptor[i]가 NULL인 경우 해당 인덱스가 비어 있음을 나타냄

            // 현재 스레드 이름(thread_current()->name)이 열려는 파일 이름(file)과 동일한 경우, 해당 파일에 대해 쓰기를 거부
            if(strcmp(thread_current()->name, file) == 0){
              file_deny_write(open_result);
            }

            // 현재 스레드의 file_descriptor[i]에 open_result를 저장하여 파일 디스크립터를 할당
            thread_current()->file_descriptor[i] = open_result;
            ret = i;
            break;
       }
    }
    lock_release(&file_lock); // file_lock을 해제하여 다른 스레드가 파일 시스템에 접근할 수 있도록 함
    return ret; // 파일 디스크립터 번호 ret를 반환하여 파일이 성공적으로 열렸음을 알림
  }
}

int filesize (int fd){
    //printf("filesize call\n");
    struct file *target = thread_current()->file_descriptor[fd]; // 현재 스레드의 파일 디스크립터 배열에서 인덱스 fd에 해당하는 파일을 가져옴
    if(target == NULL) // target이 NULL인지 확인. 
        exit(-1); // NULL인 경우 해당 파일 디스크립터에 할당된 파일이 없으므로, 잘못된 파일 디스크립터로 판단하여 오류 상태 -1로 프로세스를 종료
    else
        return file_length (target); // target이 NULL이 아니면 file_length 함수를 호출하여 해당 파일의 크기를 바이트 단위로 반환
}

int read (int fd, void *buffer, unsigned size){
//  printf("read call\n");
  check_sp_onebyone(buffer, sizeof(buffer)); // buffer가 유효한 사용자 메모리 주소인지 확인
    int ret = -1; // 읽은 바이트 수를 저장할 변수 ret를 선언하고, 기본값을 -1로 설정
    check_sp(buffer); // buffer의 메모리 주소가 유효한지 다시 한번 확인
    lock_acquire(&file_lock); // 파일에 대한 접근을 동기화하기 위해 file_lock을 획득하여 파일 시스템의 안전성을 보장

    // 파일 디스크립터가 0(표준 키보드 입력)인지 확인
    if(fd == 0){
      // 표준 입력에서 size 바이트 만큼의 데이터를 읽어옴
        for(int i = 0; i<size; i++){
          // 한 바이트씩 읽고, 읽은 바이트가 널 문자(\0)인 경우 입력이 종료되었음을 나타내고 반복문을 빠져나감
            if(input_getc() == '\0'){
                ret = i; // ret에 읽은 바이트 수를 저장
                break;
            }
        }
    }
    // 파일 디스크립터가 3에서 127 사이인지 확인
    else if(fd > 2 && fd < 128){
        // 현재 스레드의 파일 디스크립터 배열에서 fd에 해당하는 파일이 존재하는지 확인. 존재하지 않는다면 오류 반환, 프로세스 종료
        if(thread_current()->file_descriptor[fd] == NULL){
            lock_release(&file_lock);
            exit(-1);
        }
        // file_read 함수를 호출하여 파일에서 size만큼의 데이터를 buffer에 읽어, 읽은 바이트 수를 ret에 저장
        ret = file_read(thread_current()->file_descriptor[fd], buffer, size);
    }
    else{
      // 파일 디스크립터가 0이 아니면서 유효한 범위(3~127)에 있지 않으면, 유효하지 않은 파일 디스크립터로 file_lock을 해제 후 오류 반환, 프로세스 종료
      lock_release(&file_lock);
      exit(-1);
    }

    // 파일 시스템 접근이 끝났으므로 file_lock을 해제후 결과 반환
    lock_release(&file_lock);
    return ret;
}

int write (int fd, const void *buffer, unsigned size){
  //  printf("write call\n");
    check_sp_onebyone(buffer, sizeof(buffer)); // buffer가 가리키는 메모리 주소가 유효한지 확인
    check_sp(buffer+size); // buffer의 끝(buffer + size)까지 유효한 사용자 메모리인지 추가로 검사
    int ret = -1; // 반환할 바이트 수를 저장할 변수 ret를 선언하고, 기본값을 -1로 설정
    lock_acquire(&file_lock); // 파일에 대한 접근을 동기화하기 위해 file_lock을 획득

    // fd가 1인 경우 표준 출력(콘솔 출력)으로 쓰기를 수행
    if(fd == 1) {
        putbuf(buffer, size); // buffer의 데이터를 size만큼 콘솔에 출력
        ret = size; // 출력한 바이트 수를 ret에 저장
    }

    // 파일 디스크립터가 3에서 127 사이인지 확인
    else if(fd > 2 && fd < 128){

        // 현재 스레드의 파일 디스크립터 배열에서 fd에 해당하는 파일이 존재하는지 확인. 재하지 않는다면 오류 반환, 프로세스 종료
        if(thread_current()->file_descriptor[fd] == NULL){
            lock_release(&file_lock);
            exit(-1);
        }

        // 파일의 deny_write 플래그가 설정되어 있으면, file_deny_write를 호출하여 파일에 대한 쓰기를 제한
        if(thread_current()->file_descriptor[fd]->deny_write)
            file_deny_write(thread_current()->file_descriptor[fd]);
        
        // file_write 함수를 호출하여 파일에 buffer의 데이터를 size만큼 저장하고 ret에 실제로 쓴 바이트 수를 저장
        ret = file_write(thread_current()->file_descriptor[fd], buffer, size);
    }

    // 파일 디스크립터가 1이 아니면서 유효한 범위(3~127)에 있지 않으면, 유효하지 않은 파일 디스크립터로 간주하고 file_lock을 해제한 후 오류 상태 -1로 프로세스를 종료
    else{
      lock_release(&file_lock);
      exit(-1);
    }

    // 파일 시스템 접근이 끝났으므로 file_lock을 해제하여 다른 스레드가 접근할 수 있도록 하고 실제로 쓴 바이트 수가 저장된 ret 값을 반환
    lock_release(&file_lock);
    return ret;
}

void seek (int fd, unsigned position){
  //  printf("seek call\n");
    struct file *target = thread_current()->file_descriptor[fd]; // 현재 스레드의 파일 디스크립터 배열에서 fd에 해당하는 파일을 가져와 target에 저장
    if(target == NULL)
        exit(-1);
    else
        return file_seek (target, position);
}

unsigned tell (int fd){
    //printf("tell call\n");
    struct file *target = thread_current()->file_descriptor[fd]; // 현재 스레드의 파일 디스크립터 배열에서 fd에 해당하는 파일을 가져와 target에 저장

    // NULL이라면 유효하지 않은 파일 디스크립터이므로 오류 상태 -1로 프로세스를 종료
    if(target == NULL)
        exit(-1);
    
    // target이 유효하면 file_seek 함수를 호출하여 파일 포인터를 position 위치로 이동
    else
        return file_tell (target); // 파일 target의 파일 포인터를 position 위치로 설정
}

void close (int fd){
  //  printf("close call\n");
    // 파일 디스크립터 fd가 0보다 작거나 128 이상인지 확인.
    if (fd < 0 || fd >= 128)
      exit(-1);
    struct file *target = thread_current()->file_descriptor[fd]; // 현재 스레드의 파일 디스크립터 배열에서 fd에 해당하는 파일을 가져와 target에 저장

    // NULL이라면 유효하지 않은 파일 디스크립터이므로 오류 상태 -1로 프로세스를 종료
    if(target == NULL)
        exit(-1);
    // target이 유효하면 file_close 함수를 호출하여 해당 파일을 닫음
    else{
        file_close(target);
        thread_current()->file_descriptor[fd] = NULL; // 파일 디스크립터 배열에서 fd 자리를 NULL로 설정하여 해제
    }
}

void check_sp (void *uaddr){
  if (uaddr == NULL || !((unsigned int)uaddr < PHYS_BASE) || pagedir_get_page(thread_current()->pagedir, uaddr) == NULL){
    exit(-1);
  }
}
