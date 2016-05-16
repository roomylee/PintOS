#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/filesys.h"
#include "vm/page.h"
#include <stdbool.h>

#define MAX_ARGC 50 // max argument number
#define CLOSE_ALL 0 // munmap close all

tid_t process_execute (const char *);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
//Project #1
void argument_stack(char**,int,void**); // set user stack

//Project #3
struct thread *get_child_process(int); // get child process descriptor from child list
void remove_child_process(struct thread*);

//Project #4
int process_add_file(struct file*);
struct file* process_get_file(int);

//Project #8
bool handle_mm_fault(struct vm_entry* vme);
#endif /* userprog/process.h */
