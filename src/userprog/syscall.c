#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
void address_validation (void *uaddr);

struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void address_validation(void* uaddr){
  if(uaddr==NULL) exit(-1);
  if(!is_user_vaddr(uaddr)) exit(-1);
  if(!pagedir_get_page(thread_current()->pagedir, uaddr)==NULL) exit(-1);
}

static void
syscall_handler (struct intr_frame *f UNUSED) {
  void *address = f->esp;
  address_validation(address);
  uint32_t syscall_number = *(uint32_t *)address;

  // 시스템 콜 인자들을 변수에 저장
  uint32_t arg1 = *(uint32_t *)(address + 4);
  uint32_t arg2 = *(uint32_t *)(address + 8);
  uint32_t arg3 = *(uint32_t *)(address + 12);

  // 각 인자에 대한 유효성 검사
  address_validation(address + 4);
  address_validation(address + 8);
  address_validation(address + 12);

  switch(syscall_number){
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      exit(arg1);
      break;
    case SYS_EXEC:
      f->eax = exec((const char *)arg1);
      break;
    case SYS_WAIT:
      f->eax = wait((pid_t)arg1);
      break;
    case SYS_CREATE:
      f->eax = create((const char *)arg1, (unsigned)arg2);
      break;
    case SYS_REMOVE:
      f->eax = remove((const char *)arg1);
      break;
    case SYS_OPEN:
      f->eax = open((const char *)arg1);
      break;
    case SYS_FILESIZE:
      f->eax = filesize((int)arg1);
      break;
    case SYS_READ:
      f->eax = read((int)arg1, (void *)arg2, (unsigned)arg3);
      break;
    case SYS_WRITE:
      f->eax = write((int)arg1, (void *)arg2, (unsigned)arg3);
      break;
    case SYS_SEEK:
      seek((int)arg1, (unsigned)arg2);
      break;
    case SYS_TELL:
      f->eax = tell((int)arg1);
      break;
    case SYS_CLOSE:
      close((int)arg1);
      break;
    default:
      exit(-1);
  }
}

  void halt() {
  shutdown_power_off();
}

void exit(int status) {
  printf("%s: exit(%d)\n", thread_name(), status); // 종료 메세지 출력
  thread_current()->exit_status = status; // 현재 스레드(프로세스)의 exit_code에 종료 상태 코드를 저장
  thread_exit(); // 자원 해제 및 스레드 스케줄러에 제어 넘기기
}

pid_t exec (const char *file) {
    address_validation(file); // file이 가리키는 주소가 유효한 사용자 메모리 영역인지 검사
    return process_execute(file); // 지정된 파일을 실행하는 새 프로세스를 생성하고, 그 프로세스의 ID를 반환
}

int wait (pid_t pid) {
    //printf("wait call\n");
    return process_wait(pid); // 특정 자식 프로세스의 종료를 기다리고, 해당 프로세스의 종료 상태 코드를 int 타입으로 반환
}

// 파일 디스크립터가 유효한지 확인하는 함수
static bool is_valid_fd(int fd) {
    return fd > 2 && fd < 128 && thread_current()->file_descriptor[fd] != NULL;
}

// 파일 이름 유효성 검사를 위한 함수
static void validate_file_name(const char *file) {
    if (file == NULL) {
        exit(-1); // file이 NULL인 경우 프로세스 종료
    }
    address_validation(file); // 추가 메모리 유효성 검사
}

bool create(const char *file, unsigned initial_size) {
    validate_file_name(file); // 파일 이름 유효성 검사
    return filesys_create(file, initial_size); // 파일 시스템에서 새로운 파일 생성
}

bool remove(const char *file) {
    validate_file_name(file); // 파일 이름 유효성 검사
    return filesys_remove(file); // 파일 시스템에서 지정된 파일 삭제
}

int open (const char *file){

    validate_file_name(file); // 파일 이름 유효성 검사

    lock_acquire(&file_lock); // file_lock을 획득하여 파일 시스템에 대한 접근을 제어
    struct file *open_file = filesys_open(file); // 파일 시스템에서 file 이름의 파일을 열기

    // 파일을 여는 데 실패하여 open_result가 NULL인 경우 file_lock을 해제하고 -1을 반환하여 오류를 알림
    if(open_file == NULL){
        lock_release(&file_lock);
        return -1;
    }

    int ret; // 반환할 파일 디스크립터를 저장할 변수
    struct thread *cur = thread_current();

    // 파일을 여는 데 성공한 경우, 현재 스레드의 파일 디스크립터 배열에서 비어 있는 인덱스를 찾기

    for(int i = 3; i<128; i++){ // 0, 1, 2는 표준 입력, 표준 출력, 표준 오류로 예약되어 있어 3부터 사용
      if(cur->file_descriptor[i] == NULL){ // 현재 스레드의 file_descriptor[i]가 NULL인 경우 해당 인덱스가 비어 있음을 나타냄

        // 현재 스레드 이름(thread_current()->name)이 열려는 파일 이름(file)과 동일한 경우, 해당 파일에 대해 쓰기를 거부
        if(strcmp(cur->name, file) == 0) file_deny_write(open_file);

        // 현재 스레드의 file_descriptor[i]에 open_file을 저장하여 파일 디스크립터를 할당
        cur->file_descriptor[i] = open_file;
        ret = i;
        break;
      }
    }
    lock_release(&file_lock); // file_lock을 해제하여 다른 스레드가 파일 시스템에 접근할 수 있도록 함
    return ret; // 파일 디스크립터 번호 ret를 반환하여 파일이 성공적으로 열렸음을 알림

}

int filesize (int fd){
    if (!is_valid_fd(fd)) { // 파일 디스크립터 유효성 검사
        exit(-1);
    }
    struct file *target = thread_current()->file_descriptor[fd];
    return file_length(target); // 파일 크기 반환
}

// 키보드에서 데이터를 읽어오는 함수
static int read_from_keyboard(unsigned size) {
  int i;
  for (i = 0; i < size; i++) {
    if (input_getc() == '\0') { // NULL 문자를 만나면 입력 종료
      break;
    }
  }
  return i; // 읽은 바이트 수 반환
}

int read (int fd, void *buffer, unsigned size){
//  printf("read call\n");
  address_validation(buffer); // buffer가 유효한 사용자 메모리 주소인지 확인
  int ret = -1; // 읽은 바이트 수를 저장할 변수 ret를 선언하고, 기본값을 -1로 설정
  address_validation(buffer); // buffer의 메모리 주소가 유효한지 다시 한번 확인
  lock_acquire(&file_lock); // 파일에 대한 접근을 동기화하기 위해 file_lock을 획득하여 파일 시스템의 안전성을 보장

  // 파일 디스크립터가 0(표준 키보드 입력)인지 확인
  if(fd == 0){
    ret = read_from_keyboard(size);
  }
  else if (is_valid_fd(fd)) {
      ret = file_read(thread_current()->file_descriptor[fd], buffer, size);
  } 
  else {
      lock_release(&file_lock);
      exit(-1); // 유효하지 않은 fd일 경우 프로세스 종료
  }

  // 파일 시스템 접근이 끝났으므로 file_lock을 해제후 결과 반환
  lock_release(&file_lock);
  return ret;
}


// 콘솔에 쓰기
static int write_to_console(const void *buffer, unsigned size) {
    putbuf(buffer, size);
    return size;
}

// 파일에 쓰기
static int write_to_file(int fd, const void *buffer, unsigned size) {
    struct file *file = thread_current()->file_descriptor[fd];
    if (file->deny_write) {
        file_deny_write(file); // 쓰기 제한 플래그 설정
    }
    return file_write(file, buffer, size);
}


int write (int fd, const void *buffer, unsigned size){
    address_validation(buffer); // buffer가 가리키는 메모리 주소가 유효한지 확인
    address_validation(buffer+size); // buffer의 끝(buffer + size)까지 유효한 사용자 메모리인지 추가로 검사
    int ret = -1; // 반환할 바이트 수를 저장할 변수 ret를 선언하고, 기본값을 -1로 설정
    /lock_acquire(&file_lock); // 파일에 대한 접근을 동기화하기 위해 file_lock을 획득

    // fd가 1인 경우 표준 출력(콘솔 출력)으로 쓰기를 수행
    if(fd == 1) ret = write_to_console(buffer, size);
    // 파일 디스크립터가 3에서 127 사이인지 확인
    else if (is_valid_fd(fd)) {
        // 유효한 파일 디스크립터의 파일에 쓰기
        ret = write_to_file(fd, buffer, size);
    }
    else {
        // 유효하지 않은 fd일 경우 파일 잠금 해제 후 프로세스 종료
        lock_release(&file_lock);
        exit(-1);
    }

    // 파일 시스템 접근이 끝났으므로 file_lock을 해제하여 다른 스레드가 접근할 수 있도록 하고 실제로 쓴 바이트 수가 저장된 ret 값을 반환
    lock_release(&file_lock);
    
    return ret;
}

void seek(int fd, unsigned position) {
    if (!is_valid_fd(fd)) { // 파일 디스크립터 유효성 검사
        exit(-1);
    }
    
    struct file *target = thread_current()->file_descriptor[fd];
    file_seek(target, position); // 파일 포인터 이동
}

unsigned tell (int fd){

    if (!is_valid_fd(fd)) exit(-1); // 파일 디스크립터 유효성 검사

    struct file *target = thread_current()->file_descriptor[fd];
    return file_tell(target); // 현재 파일 포인터 위치 반환
}

void close(int fd) {
    if (!is_valid_fd(fd)) exit(-1); // 파일 디스크립터 유효성 검사
    
    struct file *target = thread_current()->file_descriptor[fd];
    file_close(target); // 파일 닫기
    thread_current()->file_descriptor[fd] = NULL; // 파일 디스크립터 해제
}