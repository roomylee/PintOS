#pragma once
#include <stdbool.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/off_t.h"
#define BUFFER_CACHE_ENTRY_NB 64
struct buffer_head
{
	struct inode* inode;
	bool dirty;
	bool isused;
	block_sector_t sector;
	bool clock;
	struct lock bc_lock;
	void* data;
};
bool bc_read(block_sector_t,void*,off_t,int,int);
bool bc_write(block_sector_t,void*,off_t,int,int);
struct buffer_head* bc_lookup(block_sector_t);
struct buffer_head* bc_select_victim(void);
void bc_flush_entry(struct buffer_head*);
void bc_flush_all_entries(void);
void bc_init(void);
void bc_term(void);
struct buffer_head* bc_find_unused(void);
