#include "filesys/buffer_cache.h"
#include "filesys/filesys.h"

static void* p_buffer_cache;
static struct buffer_head buffer_head[BUFFER_CACHE_ENTRY_NB];
static int clock_head;
static struct lock clock_lock;

/* Read CHUNK_SIZE data from SECTOR_IDX to BUFFER */
bool bc_read(block_sector_t sector_idx ,void* buffer,off_t bytes_read, int chunk_size, int sector_ofs)
{
	struct buffer_head* bf;
	/* Find buffer head */
	bf = bc_lookup(sector_idx);
	if( bf == NULL ) /* Cache Miss */
	{
			/* Find Unused Cache region */
			bf = bc_find_unused();
			if( bf == NULL ) bf = bc_select_victim(); /* Select Victim */
			if( bf == NULL ) return false;
			lock_acquire(&bf->bc_lock);
			bf->sector = sector_idx;
			bf->isused = true;
			block_read(fs_device, sector_idx, bf->data); /* Read from filesystem to cache memory */
			memcpy(buffer+bytes_read,bf->data+sector_ofs,chunk_size); /* Copy cache memory to buffer */
			bf->clock = true;
			lock_release(&bf->bc_lock);
	}
	else /* Cache Hit */
	{
		lock_acquire(&bf->bc_lock);
		memcpy(buffer+bytes_read,bf->data+sector_ofs,chunk_size); /* Copy cache memory to buffer */
		bf->clock = true;
		lock_release(&bf->bc_lock);
	}
	return true;
}
/* Write BUFFER to SECTOR_IDX */
bool bc_write(block_sector_t sector_idx ,void* buffer,off_t bytes_write, int chunk_size, int sector_ofs)
{
	struct buffer_head* bf;
	/* Find buffer head */
	bf = bc_lookup(sector_idx);
	if( bf == NULL ) /* Cache Miss */
	{
			/* Find Unused Cache region */
			bf = bc_find_unused();
			if( bf == NULL ) bf = bc_select_victim(); /* Select Victim */
			if( bf == NULL ) return false;
			lock_acquire(&bf->bc_lock);
			bf->sector = sector_idx;
			bf->isused = true;
			memcpy(bf->data+sector_ofs,buffer+bytes_write,chunk_size); /* Write BUFFER to cache */
			bf->clock = true;
			bf->dirty = true; /* Set dirty */
			lock_release(&bf->bc_lock);
	}
	else /* Cache Hit */
	{
		lock_acquire(&bf->bc_lock);
		memcpy(bf->data+sector_ofs,buffer+bytes_write,chunk_size); /* Write BUFFER to cache */
		bf->clock = true;
		bf->dirty = true; /* Set dirty */
		lock_release(&bf->bc_lock);
	}
	return true;
}

/* Look up buffer head which has data from SECTOR_IDX */
struct buffer_head* bc_lookup(block_sector_t sector_idx)
{
	struct buffer_head* retval;
	int i;
	for( i = 0 ; i < BUFFER_CACHE_ENTRY_NB ; i++ )
	{
			retval = &buffer_head[i];
			if(retval->sector == sector_idx ) return retval;
	}
	return NULL;
}

/* When cache area is full, Select victim block using clock algorithm */
struct buffer_head* bc_select_victim(void)
{
	struct buffer_head* victim;
	lock_acquire(&clock_lock);
	while(true) /* Clock algorithm */
	{
		victim = &buffer_head[clock_head];
		clock_head++; clock_head %= BUFFER_CACHE_ENTRY_NB;
		if(!victim->clock) break;
		else victim->clock = false;
	}
	/* Initialize cache area */
	lock_release(&clock_lock);
	if(victim->dirty) bc_flush_entry(victim);
	lock_acquire(&victim->bc_lock);
	victim->isused = false;
	victim->dirty = false;
	memset(victim->data,0,BLOCK_SECTOR_SIZE);
	lock_release(&victim->bc_lock);
	return victim;
}

/* Copy cache area to filesystem data block */
void bc_flush_entry(struct buffer_head* p_flush_entry)
{
	lock_acquire(&p_flush_entry->bc_lock);
	block_write(fs_device,p_flush_entry->sector,p_flush_entry->data);
	p_flush_entry->dirty = false;
	lock_release(&p_flush_entry->bc_lock);
}

/* Flush all entries */
void bc_flush_all_entries(void)
{
	int i = 0;
	for( i = 0 ; i < BUFFER_CACHE_ENTRY_NB ; i++ )
	{
		if(buffer_head[i].dirty)
			bc_flush_entry(&buffer_head[i]);
	}
}

/* Initialize buffer head and cache area */
void bc_init(void)
{
	int i;
	p_buffer_cache = malloc(BLOCK_SECTOR_SIZE*64);
	for( i = 0 ; i < 64 ; i++ )
	{
		buffer_head[i].dirty = false;
		buffer_head[i].isused = false;
		buffer_head[i].clock = false;
		lock_init(&buffer_head[i].bc_lock);
		buffer_head[i].data = p_buffer_cache + (BLOCK_SECTOR_SIZE*i);
		memset(buffer_head[i].data,0,BLOCK_SECTOR_SIZE);
	}
	clock_head = 0;
	lock_init(&clock_lock);
}

/* Terminate cache area */
void bc_term(void)
{
	bc_flush_all_entries();
	free(p_buffer_cache);
}

/* Find unused cache area */
struct buffer_head* bc_find_unused(void)
{
	int i;
	struct buffer_head* retval;
	for( i = 0 ; i < BUFFER_CACHE_ENTRY_NB ; i++ )
	{
		retval = &buffer_head[i];
		if(!retval->isused) return retval;
	}
	return NULL;
}
