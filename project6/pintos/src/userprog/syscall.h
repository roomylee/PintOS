#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <threads/thread.h>
#define MAX_ARGC 50
typedef int mapid_t;
void syscall_init (void);
void check_address(void*);
void get_argument(void*,int*,int);

//Project #2
void halt(void);
void exit(int);
bool create(const char*,unsigned);
bool remove(const char*);

//Project #3
tid_t exec(const char*);
int wait(tid_t);

//Project #4
int open(const char*);
int filesize(int);
int read(int,void*,unsigned);
int write(int,void*,unsigned);
void seek(int,unsigned);
unsigned tell(int);
void close(int);

//Project #8
int mmap(int,void*);
void munmap(mapid_t mapid);
#endif /* userprog/syscall.h */
