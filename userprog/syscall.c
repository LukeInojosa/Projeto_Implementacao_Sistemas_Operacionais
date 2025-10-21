#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

void
halt (void){
  shutdown_power_off();
}

void
exit (int status){
  struct thread *t = thread_current();
  t->exit_status = status;
  printf("%s: exit(%d)\n", t->name, status);

  if (t->self_child != NULL) {
    t->self_child->exit_status = status;
    t->self_child->exited = true;
    sema_up(&t->self_child->exit_sema);
  }

  thread_exit();
}

pid_t 
exec (const char* cmd_line){
  if (cmd_line == NULL) {
    return -1;
  }
  char *kpage = palloc_get_page (0);
  if (kpage == NULL) {
    return -1;
  }
  strlcpy (kpage, cmd_line, PGSIZE);

  pid_t pid = process_execute (kpage);

  if (pid == TID_ERROR) {
    palloc_free_page (kpage);
    return -1;
  }

  return pid;
}

int 
wait (pid_t pid){
  return process_wait(pid);
}
