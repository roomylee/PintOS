#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include "filesys/file.h"
static void syscall_handler (struct intr_frame *);
static struct lock stdio_lock;
static bool stdio_lock_init = false;
void 
check_address(void* addr)
{
	if( addr < (void*)0x8048000 || addr > (void*)0xc0000000 ) // check if addr in user memory or not
		exit(-1);
}
void get_argument(void* esp, int* arg, int count)
{
	int i;
	for( i = 0 ; i < count ; i++ ) // copy user stack memory to arg
	{
		check_address( &((int*)esp)[i+1] );
		check_address( &((int*)esp)[i+1] + sizeof(int));
		memcpy(&arg[i], &((int*)esp)[i+1], sizeof(int));
	}
	return;
}
void 
halt(void)
{
	shutdown_power_off();
}

void 
exit(int status)
{
	int i = 0;
	struct thread* t = thread_current(); // running thread structure
	struct file* f;
	t->exit_status = status; // save exit status
	printf("%s: exit(%d)\n",t->name,status);
	thread_exit();
}

bool 
create(const char* file,unsigned initial_size)
{
	if( strlen(file) <= 0 ) return false;
	return (bool)filesys_create(file,(off_t)initial_size);
}

bool 
remove(const char* file)
{
	return (bool)filesys_remove(file);
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

tid_t 
exec(const char* cmd_line)
{
	tid_t tid;
	struct thread* t;
	tid = process_execute(cmd_line);
	if( tid == TID_ERROR )
		return -1;
	t = get_child_process(tid);
	if( t-> load_success == 0 )
		return (tid_t)-1;
	else
		return tid;
}

int 
wait(tid_t tid)
{
	return (int)process_wait(tid);
}

/* Open file */
int 
open(const char* file)
{
	int fd;
	struct file* f = filesys_open(file);
	if( f == NULL )
		return (int)-1;
	fd = process_add_file(f);
	return (int)fd;
}

/* Return file size */
int 
filesize(int fd)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	if( f == NULL ) return (int)-1;
	else return (int)file_length(f);
}

/* Read file to buffer */
int 
read(int fd,void* buffer,unsigned size)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	unsigned char c;
	int ret_size = 0;
	if( fd == 0 ) /*Standard input*/
	{
		if(!stdio_lock_init)
		{
			stdio_lock_init = true;
			lock_init(&stdio_lock);
		}
		lock_acquire(&stdio_lock);
		while( ret_size < size )
		{
			c = input_getc();
			memcpy(buffer+ret_size,&c,sizeof(unsigned char));
			ret_size++;
		}
		lock_release(&stdio_lock);
	}
	else
	{
		ret_size = file_read(f,buffer,size);
	}
	return (int)ret_size;
}

/* Write buffer to file */
int 
write(int fd,void* buffer,unsigned size)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	int ret_size = 0;
	if( fd == 1 ) /*Standard output*/
	{
		if(!stdio_lock_init)
		{
			stdio_lock_init = true;
			lock_init(&stdio_lock);
		}
		lock_acquire(&stdio_lock);
		putbuf(buffer,size);
		ret_size = sizeof(buffer);
		lock_release(&stdio_lock);
	}
	else
	{
		ret_size = file_write(f,buffer,size); 
	}
	return (int)ret_size;
}

void 
seek(int fd,unsigned position)
{
	struct file* f = process_get_file(fd);
	if( f == NULL ) return;
	file_seek(f,position);
	return;
}

unsigned 
tell(int fd)
{
	struct file* f = process_get_file(fd);
	if( f == NULL ) return (unsigned)-1;
	return (unsigned)file_tell(f);
}

void 
close(int fd)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	if( f == NULL ) return;
	t->file_desc_table[fd] = NULL;
	file_close(f);
	return;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_num;
	int argument_arr[3];
	check_address( f-> esp );
	memcpy( &syscall_num, f->esp, sizeof(int) ); // check esp address
	//printf ("%s system call! %d\n",thread_current()->name,syscall_num);
	switch( syscall_num )
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			get_argument(f->esp,argument_arr,1);
			exit(argument_arr[0]);
			break;
		case SYS_CREATE:
			get_argument(f->esp,argument_arr,2);
			check_address((void*)argument_arr[0]); // check *file address
			f->eax = (bool)create((const char*)argument_arr[0],(unsigned)argument_arr[1]);
			break;
		case SYS_REMOVE:
			get_argument(f->esp,argument_arr,1);
			check_address((void*)argument_arr[0]); // check *file address
			f->eax = (bool)remove((const char*)argument_arr[0]);
			break;
		case SYS_EXEC:
			get_argument(f->esp,argument_arr,1);
			check_address((void*)argument_arr[0]);
			f->eax = exec((const char*)argument_arr[0]);
			break;
		case SYS_WAIT:
			get_argument(f->esp,argument_arr,1);
			f->eax = wait((tid_t)argument_arr[0]);
			break;
		case SYS_OPEN:
			get_argument(f->esp,argument_arr,1);
			check_address((void*)argument_arr[0]);
		  f->eax = open((const char*)argument_arr[0]);
			break;
		case SYS_FILESIZE:
			get_argument(f->esp,argument_arr,1);
			f->eax = filesize((int)argument_arr[0]);
			break;
		case SYS_READ:
			get_argument(f->esp,argument_arr,3);
			check_address((void*)argument_arr[1]);
			f->eax = read(argument_arr[0],argument_arr[1],argument_arr[2]);
			break;
		case SYS_WRITE:
			get_argument(f->esp,argument_arr,3);
			check_address((void*)argument_arr[1]);
			f->eax = write(argument_arr[0],argument_arr[1],argument_arr[2]);
			break;
		case SYS_SEEK:
			get_argument(f->esp,argument_arr,2);
			seek(argument_arr[0],argument_arr[1]);
			break;
		case SYS_TELL:
			get_argument(f->esp,argument_arr,1);
			f->eax = tell(argument_arr[0]);
			break;
		case SYS_CLOSE:
			get_argument(f->esp,argument_arr,1);
			close(argument_arr[0]);
			break;
		default:
			exit(-1);
	}
	return;
}
