#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
int get_argc (char *file_name);
void parse_filename (char *file_name, int argc, char **argv);
void push_stack(void **esp, char** argv, int argc);

#endif /* userprog/process.h */
