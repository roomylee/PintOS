#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include <stdbool.h>
#include "userprog/process.h"


/* Hash function that is using virtual address to hashing */
static unsigned vm_hash_func(const struct hash_elem* e,void* aux)
{
	struct vm_entry* v = hash_entry(e,struct vm_entry,elem);
	return hash_int(v->vaddr);
}

/* Less function that is used to define equality between two vm_entry in hash_find */
static bool vm_less_func(const struct hash_elem* a,const struct hash_elem* b,void* aux)
{
	struct vm_entry* va = hash_entry(a,struct vm_entry,elem);
	struct vm_entry* vb = hash_entry(b,struct vm_entry,elem);
	return va->vaddr < vb->vaddr;
}

/* Function that is used for destroy hash elem */
static void vm_destroy_func(struct hash_elem* e,void* aux)
{
	struct vm_entry* v = hash_entry(e,struct vm_entry,elem);
	free(v);	
}

/* Find vm_entry from virtual address, if there isn't return null pointer */
struct vm_entry* find_vme(void* vaddr)
{
	struct thread* t = thread_current ();
	struct vm_entry vme;
	struct vm_entry* find;
	void* addr;
	struct hash_elem* h;

	/* If hash isn't initialized, initialize hash table */
	if( !t->is_vm_init )
	{
		t->is_vm_init = true;
		vm_init(&t->vm);
	}
	addr = pg_round_down(vaddr);
	vme.vaddr = addr;	
	h = hash_find(&t->vm,&vme.elem);
	if( h == NULL ) return NULL;
	find = hash_entry( h, struct vm_entry, elem );
	return find;
}

/* Insert vm_entry to hash */
bool insert_vme(struct hash* vm,struct vm_entry* vme)
{
	struct thread* t = thread_current ();
	if( !t->is_vm_init )
	{
		t->is_vm_init = true;
		vm_init(&t->vm);
	}
	return hash_insert(vm,&vme->elem) == NULL;
}

/* Delete vm_entry from hash */
bool delete_vme(struct hash* vm,struct vm_entry* vme)
{
	return hash_delete(vm,&vme->elem) != NULL;
}

/* Initialize hash table */
void vm_init(struct hash* vm)
{
	hash_init(vm,vm_hash_func,vm_less_func,NULL);
}

/* Destroy hash table */
void vm_destroy(struct hash* vm)
{
	hash_destroy(vm,vm_destroy_func);	
}


/* load file to physical memory (Demanding Paging) */
bool load_file(void *kaddr,struct vm_entry *vme)
{
	if( file_read_at(vme->file,kaddr,vme->read_bytes,vme->offset) != vme->read_bytes )
	{
		return false;
	}
	if( vme->zero_bytes > 0 )
	{
		memset(kaddr+vme->read_bytes,0,vme->zero_bytes);
	}
	return true;
}
