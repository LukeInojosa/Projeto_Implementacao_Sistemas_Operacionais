#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"        
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "lib/kernel/stdio.h"
#include "lib/string.h"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define INITIAL_FD_CAPACITY 8
#define MAX_FILES 128

static struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);

/* Process system calls */
static void halt (void);
static void exit (int status);
static tid_t exec (const char *cmd_line);
static int wait (tid_t pid);

/* File system calls */
static int allocate_fd (struct file *file);
static struct file *get_file (int fd);
static void close_fd (int fd);
static void close_all_files (void);

static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *file);
static int sys_filesize (int fd);
static int sys_read (int fd, void *buffer, unsigned size);
static int sys_write (int fd, const void *buffer, unsigned size);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);

static bool is_valid_user_addr (const void *uaddr);

static int get_user (const uint8_t *uaddr);

static bool put_user (uint8_t *udst, uint8_t byte);

static uint32_t get_user_word (const void *uaddr);

static void validate_user_string (const char *str);

static void validate_user_buffer (const void *buffer, size_t size);

static char *copy_user_string (const char *ustr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t *esp = (uint32_t *) f->esp;

  if (!is_valid_user_addr (esp))
    exit (-1);                                   
    
  uint32_t syscall_num = get_user_word (esp);
  
  switch (syscall_num)
    {
    case SYS_HALT:
      {
        halt ();
        break;
      }
    case SYS_EXIT:
      {
        int status = (int) get_user_word (esp + 1);
        exit (status);
        break;
      }
    case SYS_EXEC:
      {
        const char *cmd_line = (const char *) get_user_word (esp + 1);
        f->eax = exec (cmd_line);
        break;
      }
    case SYS_WAIT:
      {
        tid_t pid = (tid_t) get_user_word (esp + 1);
        f->eax = wait (pid);
        break;
      }
    case SYS_CREATE:
      {
        const char *file = (const char *) get_user_word (esp + 1);
        unsigned initial_size = get_user_word (esp + 2);
        f->eax = sys_create (file, initial_size);
        break;
      }
    case SYS_REMOVE:
      {
        const char *file = (const char *) get_user_word (esp + 1);
        f->eax = sys_remove (file);
        break;
      }
    case SYS_OPEN:
      {
        const char *file = (const char *) get_user_word (esp + 1);
        f->eax = sys_open (file);
        break;
      }
    case SYS_FILESIZE:
      {
        int fd = (int) get_user_word (esp + 1);
        f->eax = sys_filesize (fd);
        break;
      }
    case SYS_READ:
      {
        int fd = (int) get_user_word (esp + 1);
        void *buffer = (void *) get_user_word (esp + 2);
        unsigned size = get_user_word (esp + 3);
        f->eax = sys_read (fd, buffer, size);
        break;
      }
    case SYS_WRITE:
      {
        int fd = (int) get_user_word (esp + 1);
        const void *buffer = (const void *) get_user_word (esp + 2);
        unsigned size = get_user_word (esp + 3);
        f->eax = sys_write (fd, buffer, size);
        break;
      }
    case SYS_SEEK:
      {
        int fd = (int) get_user_word (esp + 1);
        unsigned position = get_user_word (esp + 2);
        sys_seek (fd, position);
        break;
      }
    case SYS_TELL:
      {
        int fd = (int) get_user_word (esp + 1);
        f->eax = sys_tell (fd);
        break;
      }
    case SYS_CLOSE:
      {
        int fd = (int) get_user_word (esp + 1);
        sys_close (fd);
        break;
      }
    default:
      exit (-1);                             
    }
}

static bool
is_valid_user_addr (const void *uaddr)
{
  struct thread *cur = thread_current ();
  if (!is_user_vaddr (uaddr))
    return false;

  return pagedir_get_page (cur->pagedir, uaddr) != NULL;
}

static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

static uint32_t
get_user_word (const void *uaddr)
{
  if (!is_valid_user_addr (uaddr) ||
      !is_valid_user_addr ((const char *) uaddr + 3))
    {
      exit (-1);                               
    }
  return *(const uint32_t *) uaddr;
}

static void
validate_user_string (const char *str)
{
  if (str == NULL || !is_user_vaddr (str))
    exit (-1);                                 
    
  const char *p = str;
  while (is_valid_user_addr (p))
    {
      if (*p == '\0')
        return; /* string OK */
      p++;
    }
  exit (-1);                                 
}

static void
validate_user_buffer (const void *buffer, size_t size)
{
  if (buffer == NULL)
    exit (-1);                                  
    
  const char *buf = (const char *) buffer;
  for (size_t i = 0; i < size; i++)
    {
      if (!is_valid_user_addr (buf + i))
        exit (-1);                                
    }
}

static char *
copy_user_string (const char *ustr)
{
  if (ustr == NULL || !is_user_vaddr (ustr))
    return NULL;   

  validate_user_string (ustr);

  size_t len = 0;
  const char *p = ustr;
  while (*p != '\0')
    {
      len++;
      p++;
    }

  char *kstr = malloc (len + 1);
  if (kstr == NULL)
    return NULL;     
    
  for (size_t i = 0; i <= len; i++)
    {
      kstr[i] = ustr[i];
    }
  return kstr;
}

static int
allocate_fd (struct file *file)
{
  struct thread *cur = thread_current ();
  
  if (cur->fd_table == NULL)
    {
      cur->fd_table = malloc (INITIAL_FD_CAPACITY * sizeof (struct file *));
      if (cur->fd_table == NULL)
        return -1;
      cur->fd_capacity = INITIAL_FD_CAPACITY;
      cur->fd_count = 2;
      
      for (int i = 0; i < cur->fd_capacity; i++)
        cur->fd_table[i] = NULL;
    }
  
  for (int fd = 2; fd < cur->fd_capacity; fd++)
    {
      if (cur->fd_table[fd] == NULL)
        {
          cur->fd_table[fd] = file;
          if (fd >= cur->fd_count)
            cur->fd_count = fd + 1;
          return fd;
        }
    }
  
  if (cur->fd_capacity >= MAX_FILES)
    return -1;
    
  int new_capacity = cur->fd_capacity * 2;
  if (new_capacity > MAX_FILES)
    new_capacity = MAX_FILES;
    
  struct file **new_table = realloc (cur->fd_table, 
                                     new_capacity * sizeof (struct file *));
  if (new_table == NULL)
    return -1;
    
  for (int i = cur->fd_capacity; i < new_capacity; i++)
    new_table[i] = NULL;
    
  cur->fd_table = new_table;
  int fd = cur->fd_capacity;
  cur->fd_capacity = new_capacity;
  cur->fd_table[fd] = file;
  cur->fd_count = fd + 1;
  
  return fd;
}

static struct file *
get_file (int fd)
{
  struct thread *cur = thread_current ();
  
  if (fd < 0 || cur->fd_table == NULL || fd >= cur->fd_capacity)
    return NULL;
    
  return cur->fd_table[fd];
}

static void
close_fd (int fd)
{
  struct thread *cur = thread_current ();
  
  if (fd < 2 || cur->fd_table == NULL || fd >= cur->fd_capacity)
    return;
    
  if (cur->fd_table[fd] != NULL)
    {
      file_close (cur->fd_table[fd]);
      cur->fd_table[fd] = NULL;
    }
}

static void
close_all_files (void)
{
  struct thread *cur = thread_current ();
  
  if (cur->executable_file != NULL)
    {
      file_close (cur->executable_file);
      cur->executable_file = NULL;
    }
  
  if (cur->fd_table == NULL)
    return;
    
  for (int i = 2; i < cur->fd_capacity; i++)
    {
      if (cur->fd_table[i] != NULL)
        {
          file_close (cur->fd_table[i]);
          cur->fd_table[i] = NULL;
        }
    }
    
  free (cur->fd_table);
  cur->fd_table = NULL;
  cur->fd_capacity = 0;
  cur->fd_count = 0;
}

static bool
sys_create (const char *file, unsigned initial_size)
{
  if (file == NULL)
    exit (-1);                                

  validate_user_string (file);
  
  char *kfile = copy_user_string (file);
  if (kfile == NULL)
    return false;                               
  
  lock_acquire (&filesys_lock);
  bool success = filesys_create (kfile, initial_size);
  lock_release (&filesys_lock);
  
  free (kfile);
  return success;
}

static bool
sys_remove (const char *file)
{
  if (file == NULL)
    exit (-1);

  validate_user_string (file);
  
  char *kfile = copy_user_string (file);
  if (kfile == NULL)
    return false;
  
  lock_acquire (&filesys_lock);
  bool success = filesys_remove (kfile);
  lock_release (&filesys_lock);
  
  free (kfile);
  return success;
}

static int
sys_open (const char *file)
{
  if (file == NULL)
    exit (-1);

  validate_user_string (file);
  
  char *kfile = copy_user_string (file);
  if (kfile == NULL)
    return -1;
  
  lock_acquire (&filesys_lock);
  struct file *f = filesys_open (kfile);
  lock_release (&filesys_lock);
  
  free (kfile);
  
  if (f == NULL)
    return -1;
  
  int fd = allocate_fd (f);
  if (fd == -1)
    {
      file_close (f);
      return -1;
    }
  
  return fd;
}

static int
sys_filesize (int fd)
{
  struct file *f = get_file (fd);
  if (f == NULL)
    return -1;
    
  lock_acquire (&filesys_lock);
  off_t size = file_length (f);
  lock_release (&filesys_lock);
  
  return size;
}

static int
sys_read (int fd, void *buffer, unsigned size)
{
  if (buffer == NULL)
    exit (-1);

  validate_user_buffer (buffer, size);
  
  if (fd == STDIN_FILENO)
    {
      uint8_t *buf = (uint8_t *) buffer;
      for (unsigned i = 0; i < size; i++)
        {
          buf[i] = input_getc ();
        }
      return size;
    }
  
  struct file *f = get_file (fd);
  if (f == NULL)
    return -1;
    
  lock_acquire (&filesys_lock);
  off_t bytes_read = file_read (f, buffer, size);
  lock_release (&filesys_lock);
  
  return bytes_read;
}

static int
sys_write (int fd, const void *buffer, unsigned size)
{
  if (buffer == NULL)
    exit (-1);

  validate_user_buffer (buffer, size);
  
  if (fd == STDOUT_FILENO)
    {
      const char *buf = (const char *) buffer;
      unsigned bytes_written = 0;
      
      while (bytes_written < size)
        {
          unsigned chunk_size = size - bytes_written;
          if (chunk_size > 300)
            chunk_size = 300;
            
          putbuf (buf + bytes_written, chunk_size);
          bytes_written += chunk_size;
        }
      
      return size;
    }
  
  struct file *f = get_file (fd);
  if (f == NULL)
    return -1;
    
  lock_acquire (&filesys_lock);
  off_t bytes_written = file_write (f, buffer, size);
  lock_release (&filesys_lock);
  
  return bytes_written;
}

static void
sys_seek (int fd, unsigned position)
{
  struct file *f = get_file (fd);
  if (f == NULL)
    return;
    
  lock_acquire (&filesys_lock);
  file_seek (f, position);
  lock_release (&filesys_lock);
}

static unsigned
sys_tell (int fd)
{
  struct file *f = get_file (fd);
  if (f == NULL)
    return 0;
    
  lock_acquire (&filesys_lock);
  off_t position = file_tell (f);
  lock_release (&filesys_lock);
  
  return position;
}

static void
sys_close (int fd)
{
  close_fd (fd);
}

void
syscall_close_all_files (void)
{
  close_all_files ();
}

void
syscall_exit (int status)
{
  exit (status);
}

static void
halt (void)
{
  shutdown_power_off ();
}

static void
exit (int status)
{
  struct thread *t = thread_current();
  t->exit_status = status;
  printf("%s: exit(%d)\n", t->name, status);

  if (t->self_child != NULL) 
    {
      t->self_child->exit_status = status;
      t->self_child->exited = true;
      sema_up(&t->self_child->exit_sema);
    }

  thread_exit();
}

static tid_t 
exec (const char* cmd_line)
{
  if (cmd_line == NULL) 
    {
      exit (-1);
    }
    
  validate_user_string (cmd_line);
  
  char *kpage = palloc_get_page (0);
  if (kpage == NULL) 
    {
      return -1;
    }
    
  strlcpy (kpage, cmd_line, PGSIZE);

  tid_t pid = process_execute (kpage);

  if (pid == TID_ERROR) 
    {
      palloc_free_page (kpage);
      return -1;
    }

  return pid;
}

static int 
wait (tid_t pid)
{
  return process_wait(pid);
}
