#pragma once
#include <stdbool.h>
#include <hash.h>
#include "threads/thread.h"

/* Structure that manage virtual memory */
struct vm_entry
{
	uint8_t type;
	void *vaddr;
	bool writable;
	bool is_loaded;
	struct file* file;
	struct list_elem mmap_elem;
	size_t offset;
	size_t read_bytes;
	size_t zero_bytes;
	size_t swap_slot;
	struct hash_elem elem;
};

struct mmap_file
{
	int mapid;
	struct file* file;
	struct list_elem elem;
	struct list vme_list;
};

enum vm_type { VM_BIN,VM_FILE,VM_ANON };

struct vm_entry* find_vme(void*);
bool insert_vme(struct hash*,struct vm_entry*);
bool delete_vme(struct hash*,struct vm_entry*);
void vm_init(struct hash*);
void vm_destroy(struct hash*);
bool load_file(void*,struct vm_entry*);

