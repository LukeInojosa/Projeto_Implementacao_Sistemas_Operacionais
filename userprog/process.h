#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "lib/kernel/list.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct child_info {
  tid_t pid;
  int exit_status;
  bool exited;
  bool waited;
  struct semaphore exit_sema;
  struct list_elem elem;
};

struct load_wait{
  char *cmd_line;
  struct semaphore sema;
  bool load_success;
  struct child_info *child;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
