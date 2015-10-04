/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdlib.h>
#include <string.h>

#include "common.h"

#define	ZONE_DEFAULT_SIZE	0x80000		//512kb

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

typedef struct memblock_s
{
	int		size;           // including the header and possibly tiny fragments
	int     tag;            // a tag of 0 is a free block
	int     id;        		// should be ZONEID
	struct memblock_s       *next, *prev;
	int		pad;			// pad to 64 bit boundary
} memblock_t;

typedef struct
{
	int			size;		// total bytes malloced, including header
	memblock_t	blocklist;	// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;

//Use it instead of malloc so that if memory allocation fails,
//the program exits with a message saying there's not enough memory
//instead of crashing after trying to use a NULL pointer
void *Q_Malloc (size_t size)
{
	void *p;

	if (!(p = malloc(size)))
		Sys_Error ("Not enough memory free; check disk space");

	return p;
}

//Use it instead of calloc so that if memory allocation fails,
//the program exits with a message saying there's not enough memory
//instead of crashing after trying to use a NULL pointer
void *Q_Calloc (size_t n, size_t size)
{
	void *p;

	if (!(p = calloc(n, size)))
		Sys_Error ("Not enough memory free; check disk space");

	return p;
}

/*
==============================================================================
						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

memzone_t	*mainzone;

static void Z_ClearZone(memzone_t *zone, int size)
{
	memblock_t *block;

	// set the entire zone to one free block

	zone->blocklist.next = zone->blocklist.prev = block = (memblock_t *) ((byte *) zone + sizeof(memzone_t) );
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;

	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof(memzone_t);
}

void Z_Free (void *ptr)
{
	memblock_t	*block, *other;

	if (!ptr)
		Sys_Error ("Z_Free: NULL pointer");

	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		Sys_Error ("Z_Free: freed a pointer without ZONEID");
	if (block->tag == 0)
		Sys_Error ("Z_Free: freed a freed pointer");

	block->tag = 0;		// mark as free

	other = block->prev;
	if (!other->tag) {	// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == mainzone->rover)
			mainzone->rover = other;
		block = other;
	}

	other = block->next;
	if (!other->tag) {	// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == mainzone->rover)
			mainzone->rover = block;
	}
}

void *Z_Malloc (int size)
{
	void *buf;

	//	Z_CheckHeap ();	// DEBUG
	buf = Z_TagMalloc (size, 1);
	if (!buf)
		Sys_Error ("Z_Malloc: failed on allocation of %i bytes",size);
	memset (buf, 0, size);

	return buf;
}

void *Z_TagMalloc (int size, int tag)
{
	int extra;
	memblock_t	*start, *rover, *new, *base;

	if (!tag)
		Sys_Error ("Z_TagMalloc: tried to use a 0 tag");

	// scan through the block list looking for the first free block of sufficient size
	size += sizeof(memblock_t);	// account for size of block header
	size += 4;					// space for memory trash tester
	size = (size + 7) & ~7;		// align to 8-byte boundary

	base = rover = mainzone->rover;
	start = base->prev;

	do
	{
		if (rover == start)	// scaned all the way around the list
			return NULL;
		if (rover->tag)
			base = rover = rover->next;
		else
			rover = rover->next;
	} while (base->tag || base->size < size);

	// found a block big enough
	extra = base->size - size;
	if (extra >  MINFRAGMENT) {	// there will be a free fragment after the allocated block
		new = (memblock_t *) ((byte *)base + size );
		new->size = extra;
		new->tag = 0;			// free block
		new->prev = base;
		new->id = ZONEID;
		new->next = base->next;
		new->next->prev = new;
		base->next = new;
		base->size = size;
	}

	base->tag = tag;				// no longer a free block

	mainzone->rover = base->next;	// next allocation will start looking here

	base->id = ZONEID;

	// marker for memory trash testing
	*(int *)((byte *)base + base->size - 4) = ZONEID;

	return (void *) ((byte *)base + sizeof(memblock_t));
}

//============================================================================


void Memory_Init()
{
	int p, zonesize = ZONE_DEFAULT_SIZE;

	if ((p = COM_CheckParm ("-zone")) && p + 1 < com_argc)
		zonesize = Q_atoi (com_argv[p + 1]) * 1024;

	mainzone = malloc(zonesize);
	if (mainzone == 0)
		Sys_Error("Unable to allocate zone memory\n");

	Z_ClearZone (mainzone, zonesize);
}

void Memory_Shutdown()
{
	free(mainzone);
}

