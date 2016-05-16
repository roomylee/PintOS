#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

void halt (void);               // 0: SYS_HALT
void exit (int);                // 1: SYS_EXIT
tid_t exec (const char*);       // 2: SYS_EXEC
int wait (tid_t);               // 3: SYS_WAIT
bool create (const char*,unsigned);  //4: SYS_CREATE
bool remove (const char *); 	     //5: SYS_REMOVE
int open (const char*);         // 6: SYS_OPEN
int filesize (int);		// 7: SYS_FILESIZE
int read (int, void*,unsigned); // 8: SYS_READ
int write (int,void*,unsigned); // 9: SYS_WRITE
void seek (int,unsigned);	// 10 SYS_SEEK
unsigned tell (int);		// 11 SYS_TELL
void close (int);		// 12 SYS_CLOSE

struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_address(void *addr){

  if(addr<(void*)0x08048000||addr>=(void*)0xc0000000){
   exit(-1);
  }
}

void get_argument(void *esp, int *arg,int count){
  int i=0;
  int *arg_addr;

  for(i=0;i<count;i++){
   arg_addr=(int*)esp+i+1;
   arg[i]=arg_addr;
   check_address((void*)arg_addr);
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t *stack_ptr = f->esp;
  int arg[3];
  int number = *stack_ptr;


  /*check address  */
  check_address(stack_ptr);
  /*if arg is pointer */
  if(stack_ptr){
   
   switch(number){
    case SYS_HALT:
     get_argument(stack_ptr,&arg[0],1);
     halt();
    break;

    case SYS_EXIT:
     get_argument(stack_ptr,&arg[0],2);
     exit(*(int*)arg[0]);
    break;

    case SYS_EXEC:
     get_argument(stack_ptr,&arg[0],2);
     f->eax=exec((const char*)*(int*)arg[0]);
     break;
    
    case SYS_WAIT:
     get_argument(stack_ptr,&arg[0],2);
     f->eax=wait(*(int*)arg[0]);
     break;

    case SYS_CREATE:
    get_argument(stack_ptr,&arg[0],3);
    f->eax=create((const char*)*(int*)arg[0],(unsigned)*(int*)arg[1]);
    break;

    case SYS_REMOVE:
    get_argument(stack_ptr,&arg[0],3);
    f->eax=remove((char*)*(int*)arg[0]);
    break;
    
    case SYS_OPEN:
    get_argument(stack_ptr,&arg[0],2);
    f->eax=open((const char*)*(int*)arg[0]);
    break;

    case SYS_FILESIZE:
    get_argument(stack_ptr,&arg[0],2);
    f->eax=filesize(*((int*)arg[0]));
    break;

    case SYS_READ:
    //check_address(arg[1]);
    get_argument(stack_ptr,&arg[0],4);
    check_address((void*)*(int*)arg[1]);
    f->eax=read(*(int*)arg[0],(void*)*(int*)arg[1],(unsigned)*(int*)arg[2]);
    break;

    case SYS_WRITE:
    get_argument(stack_ptr,&arg[0],4);
    f->eax=write(*(int*)arg[0],(void*)*(int*)arg[1],(unsigned)*(int*)arg[2]);
    break;

    case SYS_SEEK:
    get_argument(stack_ptr,&arg[0],3);
    seek(*(int*)arg[0],(unsigned)*(int*)arg[1]);
    break;

    case SYS_TELL:
    get_argument(stack_ptr,&arg[0],2);
    f->eax=tell(*(int*)arg[0]);
    break;
   
    case SYS_CLOSE:
    get_argument(stack_ptr,&arg[0],2);
    close(*(int*)arg[0]);
    break;

    default:
     thread_exit();
    break;
    }//switch end

  }
  else{
   thread_exit();
  }
}

void halt(void){
  shutdown_power_off();
}

void exit(int status){
  struct thread *cur = thread_current();

  cur->exit_status=status;
  printf("%s: exit(%d)\n",cur->name,status);
  thread_exit();
}

tid_t exec (const char* cmd_line){
  tid_t tid=process_execute(cmd_line);
  struct thread *child= get_child_process(tid);

  sema_down(&child->load_sema);
  if(child->loaded==1)
   return tid;
  else
   return -1;
}

int wait (tid_t tid){
  return process_wait(tid);
}


bool create(const char *file, unsigned initial_size){
  if(file&&initial_size>=0)
   return filesys_create(file,initial_size);
  else
   exit(-1);
}


bool remove (const char *file){
  return filesys_remove(file);
}

int open (const char* file){
  if(file==NULL)
   return -1;
  
  struct file *fp=filesys_open(file);
  int fd=-1;

  if(fp){
   fd=process_add_file (fp);
  }
  else
   return -1;

  return fd;
}

int filesize (int fd){
  struct file *fp = process_get_file(fd);
 
  if(fp)
   return (int) file_length(fp);
  else
   return -1;
}

int read (int fd, void *buffer, unsigned size){
  lock_acquire(&file_lock);
  if(fd==0){
   int i=0;
   uint8_t *local_buf=(uint8_t *) buffer;
   for(i=0;i<size;i++){
    local_buf[i]=input_getc();
   }
   lock_release(&file_lock);
   return size;
  }

  struct file *fp = process_get_file(fd);
  int byte=0;

  if(!fp){
   lock_release(&file_lock);
   return -1;
  }
  
  byte=file_read(fp,buffer,size);
  lock_release(&file_lock);
  return byte;
}

int write (int fd, void *buffer, unsigned size){

  lock_acquire (&file_lock);
  if(fd==1){
   putbuf(buffer,size);
   lock_release(&file_lock);
   return size;
  }

  struct file *fp = process_get_file(fd);
  int byte=0;

  if(!fp){
   lock_release(&file_lock);
   return -1;
  }

  byte=(int)file_write(fp,buffer,size);
  lock_release (&file_lock);
  return byte;
}

void seek (int fd, unsigned position){
  struct file *fp = process_get_file(fd);
  
  if(fp)
   file_seek (fp,position);
}

unsigned tell (int fd){
  struct file *fp= process_get_file(fd);
  if(!fp){
   lock_release (&file_lock);
   return -1;
  }

  return file_tell(fp);
}

void close (int fd){
  if(fd==0||fd==1)
   exit(-1);
  process_close_file(fd);
}
