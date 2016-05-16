#include "userprog/syscall.h"
#include "userprog/process.h"
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
#include "vm/page.h"
#include "threads/vaddr.h"
static void syscall_handler (struct intr_frame *);
static struct lock file_lock;
static bool file_lock_init = false;
void 
check_address(void* addr)
{
	if( addr < (void*)0x8048000 || addr > (void*)0xc0000000 ) // check if addr in user memory or not
	{
		exit(-1);
	}
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
	bool success = false;
	if( strlen(file) <= 0 ) return false;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	success = filesys_create(file,(off_t)initial_size);
	lock_release(&file_lock);
	return success;
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
	struct file* f;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	f = filesys_open(file);
	//printf("file : %s \t f : %#lx\n",file,f);
	if( f == NULL )
	{
		lock_release(&file_lock);
		return (int)-1;
	}
	fd = process_add_file(f);
	lock_release(&file_lock);
	return (int)fd;
}

/* Return file size */
int 
filesize(int fd)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	int file_len;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	if( f == NULL ) file_len = -1;
	else file_len = file_length(f);
	lock_release(&file_lock);
	return file_len;
}

/* Read file to buffer */
int 
read(int fd,void* buffer,unsigned size)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	unsigned char c;
	int ret_size = 0;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	if( fd == 0 ) /*Standard input*/
	{
		while( ret_size < size )
		{
			c = input_getc();
			memcpy(buffer+ret_size,&c,sizeof(unsigned char));
			ret_size++;
		}
	}
	else
	{
		ret_size = file_read(f,buffer,size);
	}
	lock_release(&file_lock);
	return (int)ret_size;
}

/* Write buffer to file */
int 
write(int fd,void* buffer,unsigned size)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	int ret_size = 0;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	if( fd == 1 ) /*Standard output*/
	{
		putbuf(buffer,size);
		ret_size = sizeof(buffer);
	}
	else
	{
		ret_size = file_write(f,buffer,size); 
	}
	lock_release(&file_lock);
	return (int)ret_size;
}

void 
seek(int fd,unsigned position)
{
	struct file* f = process_get_file(fd);
	if( f == NULL ) return;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	file_seek(f,position);
	lock_release(&file_lock);
	return;
}

unsigned 
tell(int fd)
{
	struct file* f = process_get_file(fd);
	unsigned ofs;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	if( f == NULL ) ofs = -1;
	else ofs = file_tell(f);
	lock_release(&file_lock);
	return ofs;
}

void 
close(int fd)
{
	struct thread* t = thread_current();
	struct file* f = t->file_desc_table[fd];
	if( f == NULL ) return;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	t->file_desc_table[fd] = NULL;
	file_close(f);
	lock_release(&file_lock);
	return;
}

/* Load file data to memory (Demanding Paging) */
int
mmap(int fd,void*addr)
{
	struct thread* t = thread_current();
	struct file* f = process_get_file(fd);
	struct file* fre;
	struct vm_entry* vme;
	size_t f_size;
	struct mmap_file* mmf;

	if( f == NULL ) return -1;
	if( (int)addr % PGSIZE || addr < (void*)0x8048000 || addr > (void*)0xc0000000 ) // check if addr in user memory or not
	{
		return -1;
	}
	vme = find_vme(addr);
	if( vme != NULL ) return -1;
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	fre = file_reopen(f); // Copy file descriptor
	f_size = file_length(fre);
	if( f_size < 1 )
	{
		lock_release(&file_lock);
		return -1;
	}
	mmf = malloc(sizeof(struct mmap_file)); // Initailize mmap file
	mmf->mapid = t->mapid++;
	mmf->file = fre;
	list_init(&mmf->vme_list);
	list_push_back(&t->mmap_list,&mmf->elem);
	off_t ofs = 0;
	while( f_size > 0 ) // Initialize vm entry 
	{
     size_t page_read_bytes = f_size < PGSIZE ? f_size : PGSIZE;
     size_t page_zero_bytes = PGSIZE - page_read_bytes;
		 struct vm_entry* v = malloc(sizeof(struct vm_entry));
		 v->type = VM_FILE;
		 v->vaddr = addr;
		 v->writable = true;
		 v->is_loaded = false;
		 v->file = fre;
		 list_push_back(&mmf->vme_list,&v->mmap_elem);
		 v->offset = ofs;
		 v->read_bytes = page_read_bytes;
		 v->zero_bytes = page_zero_bytes;
		 insert_vme(&t->vm,v); // Insert to vm entry hash table

		 ofs += page_read_bytes;
		 f_size -= page_read_bytes;
		 addr += PGSIZE;
	}
	lock_release(&file_lock);
	return mmf->mapid;
}

/* Free vm_entry which have same mapid value with parameter */
void
munmap(mapid_t mapid)
{	
  struct list_elem *e;
	struct thread* cur = thread_current();	
	if( !file_lock_init )
	{
		file_lock_init = true;
		lock_init(&file_lock);
	}
	lock_acquire(&file_lock);
	for (e = list_begin (&cur->mmap_list); e != list_end (&cur->mmap_list);) // free mmap file and vm_entry that are belong to
    {
			struct mmap_file *mmf = list_entry (e, struct mmap_file, elem);
			if( mapid == CLOSE_ALL || mmf->mapid == mapid )
			{
				do_munmap(mmf);
			}
			e = list_next(e);
			file_close(mmf->file);
			list_remove(&mmf->elem);
			free(mmf);
    }
	lock_release(&file_lock);
}

void
do_munmap(struct mmap_file* mmap_file) // Free vm entry
{
	struct list_elem* ee;
	struct thread* t = thread_current();
	for( ee = list_begin(&mmap_file->vme_list) ; ee != list_end(&mmap_file->vme_list) ; )
	{
		struct vm_entry* vme = list_entry(ee, struct vm_entry, mmap_elem);
		if( vme->is_loaded && pagedir_is_dirty(t->pagedir,vme->vaddr) )
		{
			void *kaddr = pagedir_get_page(t->pagedir,vme->vaddr);
			file_write_at(vme->file,kaddr,vme->read_bytes,vme->offset);
		}
		ee = list_next(ee);
		list_remove(&vme->mmap_elem); // Remove from mmap file
		list_remove(&vme->elem); // Remove from vm entry list
		free(vme);
	}
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
		case SYS_MMAP:
			get_argument(f->esp,argument_arr,2);
			//check_address(argument_arr[1]);
			f->eax = mmap(argument_arr[0],argument_arr[1]);
			break;
		case SYS_MUNMAP:
			get_argument(f->esp,argument_arr,1);
			munmap(argument_arr[0]);
			break;
		default:
			exit(-1);
	}
	return;
}
