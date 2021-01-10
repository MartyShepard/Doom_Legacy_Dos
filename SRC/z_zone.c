// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: z_zone.c 577 2009-11-30 03:32:36Z wesleyjohnson $
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Portions Copyright (C) 1998-2000 by DooM Legacy Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// $Log: z_zone.c,v $
// Revision 1.17  2002/07/29 21:52:25  hurdler
// Someone want to have a look at this bugs
//
// Revision 1.16  2001/06/30 15:06:01  bpereira
// fixed wronf next level name in intermission
//
// Revision 1.15  2001/03/13 22:14:20  stroggonmeth
// Long time no commit. 3D floors, FraggleScript, portals, ect.
//
// Revision 1.14  2001/01/25 22:15:44  bpereira
// added heretic support
//
// Revision 1.13  2000/11/06 20:52:16  bpereira
// no message
//
// Revision 1.12  2000/11/03 13:15:13  hurdler
// Some debug comments, please verify this and change what is needed!
//
// Revision 1.11  2000/11/02 17:50:10  stroggonmeth
// Big 3Dfloors & FraggleScript commit!!
//
// Revision 1.10  2000/10/14 18:33:34  hurdler
// sorry, I forgot to put an #ifdef for hw memory report
//
// Revision 1.9  2000/10/14 18:32:16  hurdler
// sorry, I forgot to put an #ifdef for hw memory report
//
// Revision 1.8  2000/10/04 16:33:54  hurdler
// Implement hardware texture memory stats
//
// Revision 1.7  2000/10/02 18:25:45  bpereira
// no message
//
// Revision 1.6  2000/08/31 14:30:56  bpereira
// no message
//
// Revision 1.5  2000/07/01 09:23:49  bpereira
// no message
//
// Revision 1.4  2000/04/30 10:30:10  bpereira
// no message
//
// Revision 1.3  2000/04/24 20:24:38  bpereira
// no message
//
// Revision 1.2  2000/02/27 00:42:11  hurdler
// fix CR+LF problem
//
// Revision 1.1.1.1  2000/02/22 20:32:32  hurdler
// Initial import into CVS (v1.29 pr3)
//
//
// DESCRIPTION:
//      Zone Memory Allocation. Neat.
//
//-----------------------------------------------------------------------------


#include "doomdef.h"
#include "z_zone.h"
#include "i_system.h"
#include "command.h"
#include "m_argv.h"
#include "i_video.h"
#include "doomstat.h"
#ifdef HWRENDER
#include "hardware/hw_drv.h" // for hardware memory stats
#endif

// [WDJ] 1/24/2009 Memory control defines.
// Amount of main memory to allocate, in MB
// Original values
//#define NORM_MAIN_MEM_MB		6
//#define MAX_MAIN_MEM_MB		20

// Larger values, FreeDoom ran out of space
// FreeDoom MAP10 uses 23MB, with fragmentation requires 30MB,
// but will run in 10MB when forced.
#define MIN_MAIN_MEM_MB		 8
#define NORM_MAIN_MEM_MB	24
#define MAX_MAIN_MEM_MB		80
#define GROW_MIN_MAIN_MEM_MB	 4
#define GROW_MAIN_MEM_MB	 2
//#define GROW_MAIN_MEM_MB	 8

// Choose one (and only one) memory system.
// [WDJ] Because of the widely varying systems that Legacy can run on, it is
// desirable to maintain all these variations, to give each user one that fits
// well with their system and preferences.

// [WDJ] Plain allocation from malloc works for systems with large memory.
// Gives a reference performance, can be used to differentiate tagging problems.
// When the user writes out-of-bounds of malloced region it will do a sigsegv.
// It does not use the tags, cannot recover PU_CACHE, PU_LEVEL, etc. memory.
// Uses the most memory of all choices.
//#define PLAIN_MALLOC

// [WDJ] Combination of malloc and tags.
// Does malloc from heap, so will grow from heap as use increases.
// Maintains zone block lists and will free blocks according to tags.
//#define TAGGED_MALLOC

// ZONE_ZALLOC, the default based on the original zone memory.
// It allocates a huge block at program start and makes all Z_Malloc
// allocations from it.
// Has a command line option to set the zone block size ( -mb <int> ).
// It also has some conditional experimental code.
#define ZONE_ZALLOC

// Grows the initial allocation block when it runs out of memory.
// Runs in the smallest memory of all the choices.
// Uses tags, and recovers PU_CACHE and PU_LEVEL memory first.
// Applied as an option to ZONE_ZALLOC.
#define GROW_ZONE

// Conditional code that reduces fragmentation at the expense of a much
// longer search through memory.
// Applied as an option to ZONE_ZALLOC.
//#define ALLOC_FRAG1

// Aggressively purges any PU_CACHE, clearing cache faster.
// This stresses the memory system more, testing user code to not
// depend upon PU_CACHE that can disappear. Used for testing memory code.
// Applied as an option to ZONE_ZALLOC.
//#define AGGRESSIVE_PURGE

// [WDJ] 1/22/2009 For some crazy reason this code said that ->user==NULL
// is an free block, while almost every call to Z_ALLOC has user=NULL.
// Fixed to use tag=PU_FREE instead.


// default, and prevention of crossed definitions
#if ! (defined( PLAIN_MALLOC ) || defined( TAGGED_MALLOC ) )
#ifndef ZONE_ZALLOC
#define ZONE_ZALLOC
#endif
#endif

#ifdef PLAIN_MALLOC
  #undef ZONE_ZALLOC
  #undef ALLOC_FRAG1
  #undef GROW_ZONE
  #define USE_MALLOC
#endif

#ifdef TAGGED_MALLOC
  #undef ZONE_ZALLOC
  #undef PLAIN_MALLOC
  #undef ALLOC_FRAG1
  #undef GROW_ZONE
  #define USE_MALLOC
#endif


// =========================================================================
//                        ZONE MEMORY ALLOCATION
// =========================================================================
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of little value to free a cachable block (for ZONE memory),
//  because it will get overwritten automatically if needed.
//

typedef struct
{
    // start / end cap for linked list
    // prevents free block combines around end of blocklist
    memblock_t  blocklist;
   
    // total bytes malloced, including header
    ULONG	size;

    memblock_t* rover;

} memzone_t;

static memzone_t* mainzone;

#ifdef USE_MALLOC
memzone_t	memhead;	// statically allocated mainzone
#endif

#define ZONEID  0x1d4a11


void Command_Memfree_f( void );


#if defined(TAGGED_MALLOC) || defined(ZONE_ZALLOC)
// Used to insert a new block between prv and nxt.
// ZONE_ZALLOC: Caller must make sure the blocks are logically contiguous.
static void Z_LinkBlock(memblock_t *block, memblock_t *prv, memblock_t *nxt)
{
  block->prev = prv;
  block->next = nxt;
  nxt->prev = prv->next = block;
}
#endif

#ifdef USE_MALLOC
// PLAIN_MALLOC returns blocks to the heap.
// TAGGED_MALLOC unlinks blocks and returns them to the heap.
static void Z_UnlinkFree(memblock_t *block)
{
#ifdef TAGGED_MALLOC
  register memblock_t * prv = block->prev;
  register memblock_t * nxt = block->next;

  block->prev->next = nxt;
  block->next->prev = prv;
#endif
  memhead.size -= block->size;
  free(block);
}
#endif

#ifdef ZONE_ZALLOC
// Zone marks blocks as free and combines them into large free blocks.

// mark the block free
static void Z_MarkFree(memblock_t *block)
{
  block->tag = PU_FREE; // free block mark
  block->id = 0;
  block->user = NULL;
}

// mark a block free, try to combine it with neighboring free blocks
static void Z_CombineFreeBlock(memblock_t *block)
{
 
  Z_MarkFree( block );  // mark as a free block

  // see if previous block is free, if so, merge blocks
  memblock_t *other = block->prev;
  if (other->tag == PU_FREE)
  {
      other->size += block->size;
      other->next = block->next;
      other->next->prev = other;

      if (block == mainzone->rover)	// rover block is gone, fix rover
	mainzone->rover = other;

      block = other;
  }

  // see if next block is free, if so, merge blocks
  other = block->next;
  if (other->tag == PU_FREE)
  {
      block->size += other->size;
      block->next = other->next;
      block->next->prev = block;

      if (other == mainzone->rover)	// rover block is gone, fix rover
	mainzone->rover = block;
  }
}


static unsigned int mb_used = NORM_MAIN_MEM_MB;

//
// Z_ZoneInit
//
// Init zone without consideration for existing allocations.
void Z_ZoneInit ( int mb_zonesize )
{
    memblock_t*         block;
    int zonesize = mb_zonesize<<20;
   
    // make the allocation of the huge memory block
    mainzone = (memzone_t *) malloc(zonesize);
    if( !mainzone ) {
         I_Error("Could not allocate %d megabytes.\n"
                 "Please use -mb parameter and specify a lower value,\n"
		 "use a smaller video size, and/or a smaller wad.",
		 mb_zonesize);
    }

    mb_used = mb_zonesize;	// save for some stats later
   
    // touch memory to stop swaping
//    memset(mainzone, 0, zonesize);
    // [WDJ] This did not stop swapping, just made pages dirty so they must be swapped out.
    // If user specifies large -mb value, then it would dirty a large unused memory area.
    memset(mainzone, 0, min(zonesize, 16182));  // [WDJ] dirty only the first 16K
   
//    if( M_checkParm("-lock") )
//        I_LockMemory(mainzone);

    // setup the zone
    mainzone->size = zonesize;
    // setup the block head as protected
    Z_MarkFree( &mainzone->blocklist );	// Init as free
    mainzone->blocklist.tag = PU_ZONE;	// protect head against free combine
   
    // set the entire zone to one free block
    block = (memblock_t *)( (byte *)mainzone + sizeof(memzone_t) );
    block->size = zonesize - sizeof(memzone_t);
    Z_MarkFree( block );  // init free block

    // init circular linking
    Z_LinkBlock(block, &mainzone->blocklist, &mainzone->blocklist);
   
    mainzone->rover = block;
}

#ifdef GROW_ZONE
// Grow the ZONE_ZALLOC mainzone allocation
// Return first block of new free zone
memblock_t*  Z_GrowZone( int reqsize, int std_grow )
{
    // Grow the zone memory
    int  min_rsize = reqsize + (16*sizeof(memblock_t));
   	// reqsize + block header + zone header + extra
    int  grow_size = max( std_grow, max( min_rsize, 2048 ) );
    memblock_t * addzone = (memblock_t *) malloc(grow_size);
    if( addzone ) {
        // Create a guard to prevent combining with blocks outside
        // this zone; the allocations are not contiguous
        memblock_t * freezone = addzone + 1;	// next memblock
        // first memblock as static guard
        Z_MarkFree( addzone );
        addzone->tag = PU_ZONE;	// untouchable guard
        addzone->size = sizeof(memblock_t);
        // second memblock has remaining space as free block
        Z_MarkFree( freezone );
        freezone->size = grow_size - sizeof(memblock_t);
        // link as last block in existing zone
        // then will be part of normal search without extra coding
        Z_LinkBlock( addzone, mainzone->blocklist.prev, &mainzone->blocklist ); 
        Z_LinkBlock( freezone, addzone, &mainzone->blocklist ); 

        mainzone->size += grow_size;
        mb_used = mainzone->size >> 20;	// to MiB
		   
        fprintf(stderr,"Z_Malloc: Grow by %i KiB, total=%i MiB\n",
			   grow_size>>10, mb_used );
        return freezone;
    }else{
       return NULL;
    }
}
#endif
#endif	// ZONE_ZALLOC


//
// Z_Init
//
void Z_Init (void)
{
#ifdef USE_MALLOC
    // does not need to pre-allocate memory
    mainzone = & memhead;	// static
    // setup the linked list, rest of mainzone is unused
    memhead.rover = NULL;
    memhead.size = 0;
    memhead.blocklist.tag = PU_ZONE; // mark head, no combines anyway
#ifdef TAGGED_MALLOC
    // init circular linking, TAGGED needs the lists
    memhead.blocklist.next = memhead.blocklist.prev = &memhead.blocklist;
#else
    // no lists in PLAIN
    memhead.blocklist.next = memhead.blocklist.prev = NULL;
#endif
#else
// ZONE_ZALLOC
    int         mb_wanted = 10;

    if( M_CheckParm ("-mb") )
    {
        if( M_IsNextParm() )
            mb_wanted = atoi (M_GetNextParm());
        else
            I_Error("usage : -mb <numbers of megabyte for the heap>");
    }
    else
    {
        ULONG       freemem, total;
        freemem = I_GetFreeMem(&total)>>20;
        total >>= 20;	// MiB
        CONS_Printf("system memory %d MiB free %d MiB\n",
		    (int)total, (int)freemem);
        // We assume that system uses a lot of memory for disk cache.
        // Can ask for more than freemem and get it from disk cache.
	// MEM consts are now defined above, [WDJ]
#ifdef GROW_ZONE
        // initial zone memory, will grow when needed
        mb_wanted = min(min( GROW_MIN_MAIN_MEM_MB, freemem ), MAX_MAIN_MEM_MB);
        if( mb_wanted < 2 )   mb_wanted = 2;
#else
        // zone memory, all at once, only get one try
        mb_wanted = min( NORM_MAIN_MEM_MB, (total/2) );  // reasonable limit
        if( freemem < NORM_MAIN_MEM_MB )
            freemem = (freemem + total)/2;	// ask for compromise
        mb_wanted = min(max(freemem, mb_wanted), MAX_MAIN_MEM_MB);
        if( mb_wanted < MIN_MAIN_MEM_MB )
	    mb_wanted = MIN_MAIN_MEM_MB;
#endif       
    }
    // [WDJ] mem limited to 2047 MB by 32bit int
    if( mb_wanted > 2047 )   mb_wanted = 2047;	// [WDJ]
    CONS_Printf ("%d megabytes requested for Z_Init.\n", mb_wanted);
    Z_ZoneInit( mb_wanted );
#endif

    // calls Z_Malloc, so must be last
    COM_AddCommand ("memfree", Command_Memfree_f);
}


//
// Z_Free
//
#ifdef ZDEBUG
void Z_Free2(void* ptr, char *file, int line)
#else
void Z_Free (void* ptr)
#endif
{
    memblock_t*  block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

#ifdef ZDEBUG
   memblock_t*         other;
   // SoM: HARDERCORE debuging
   // Write all Z_Free's to a debug file
   if(debugfile)
     fprintf(debugfile, "ZFREE@File: %s, line: %i\n", file, line);
   //BP: hardcore debuging
   // check if there is not a user in this zone
for (other = mainzone->blocklist.next ; other->next != &mainzone->blocklist; other = other->next)
{
   if((other!=block) &&
      other->tag &&
      (other->user>(void **)0x100) &&
      ((other->user)>=(void **)block) &&
      ((other->user)<=(void **)((byte *)block)+block->size) )
   {
       //I_Error("Z_Free: Pointer in zone\n");
       I_Error("Z_Free: Pointer %s:%d in zone at %s:%i",other->ownerfile,other->ownerline,file,line);
   }
}
#endif

    if (block->id != ZONEID)
        I_Error ("Z_Free: memory block has corrupted ZONEID");
#ifdef PARANOIA
    // get direct a segv when using a pointer that isn't right
    memset(ptr,0,block->size-sizeof(memblock_t));
#endif
   
    if (block->user > (void **)0x100)
    {
        // smaller values are not pointers
        // Note: OS-dependent?

        // clear the user's owner ptr (they no longer have access)
        *block->user = NULL;
    }
   
#ifdef USE_MALLOC
    Z_UnlinkFree(block);
#else
// ZONE_ZALLOC
    Z_CombineFreeBlock( block );
#endif
}


#if defined(TAGGED_MALLOC) || defined(ZONE_ZALLOC)
// Purge a block, conditionally
void Z_Purge( memblock_t* block )
{
    if( block->tag >= PU_PURGELEVEL ) {
        // purge the block
        Z_Free((byte *)block + sizeof(memblock_t));
    }
}
#endif


#ifdef USE_MALLOC
//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#ifdef ZDEBUG
void*   Z_Malloc2 (int reqsize, memtag_e tag, void *user, int alignbits,
		   char *file, int line)
#else
void* Z_MallocAlign (int reqsize, memtag_e tag, void *user, int alignbits )
#endif
{
    memblock_t* newblock;
    int   memalloc_size;	// with the memalloc header
   
    if( tag == PU_FREE ){
       fprintf(stderr,"Z_ALLOC called with PU_FREE tag, conflict with FREE BLOCK\n");
       tag = PU_LEVEL;      // choose safe interpretation
       // tag = PU_DAVE;	// if must debug
    }

    if( tag == PU_CACHE_DEFAULT )   tag = PU_CACHE;

    reqsize = (reqsize + 3) & ~3;	// alloc rounded up to next 4 byte alignment
    // account for size of block header
    memalloc_size = reqsize + sizeof(memblock_t);

    newblock = malloc(memalloc_size);
    if( newblock == NULL ){
       I_Error ("Z_Malloc: malloc failed on allocation of %i bytes\n");
    }
    memhead.size += memalloc_size;
#ifdef TAGGED_MALLOC   
    // link at head of list
    Z_LinkBlock( newblock, &memhead.blocklist, memhead.blocklist.next);
    // convert tags that are not used to ones that are handled
    if( tag >= PU_PURGELEVEL )   tag = PU_LEVEL;
#endif
    newblock->tag = tag;
    newblock->id = ZONEID;
    newblock->user = user;
    newblock->size = memalloc_size;
    void* basedata = (byte*)newblock + sizeof(memblock_t);
    if(user) *(void **)user = basedata;
    return basedata;
}

#else
// ZONE_ZALLOC

//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#define MINFRAGMENT             sizeof(memblock_t)

#ifdef ZDEBUG
void*   Z_Malloc2 (int reqsize, memtag_e tag, void *user, int alignbits,
		   char *file, int line)
#else
void* Z_MallocAlign (int reqsize, memtag_e tag, void *user, int alignbits )
#endif
{
    memblock_t* start;	// marks start of search
    memblock_t* rover;	// walks through block list
    memblock_t* base = NULL;	// [WDJ] points to usable memory, or is NULL
    int   memalloc_size;	// with the memalloc header
    int   basesize = 0;	// accumulate blocks
    int   basedata;
    int   misalign = 0;	// align mismatch
    int	  tries = 0;	// [WDJ] Try multiple passes before bombing out
      // [WDJ] TODO: could compact memory after first try
      // 1. Call owners of memory to reallocate, and clean up ptrs.
      // 2. Let tag give permission to move blocks and update user ptr.
    
    ULONG alignmask=(1<<alignbits)-1;
#define ALIGN(a) (((ULONG)a+alignmask) & ~alignmask)

   
// ZONE_ZALLOC
    if( tag == PU_FREE ){
       fprintf(stderr,"Z_ALLOC called with PU_FREE tag, conflict with FREE BLOCK\n");
       tag = PU_LEVEL;      // choose safe interpretation
       // tag = PU_DAVE;	// if must debug
    }

    if( tag == PU_CACHE_DEFAULT )   tag = PU_CACHE;

    reqsize = (reqsize + 3) & ~3;	// alloc rounded up to next 4 byte alignment
    // account for size of block header
    memalloc_size = reqsize + sizeof(memblock_t);

    // scan through the block list,
    // looking for the first free block of sufficient size,
   
   // [WDJ] 1/22/2009  MODIFIED ZONE_ZALLOC
   // This also has experimental code blocks, which are currently disabled.

#ifdef ALLOC_FRAG1
 // [WDJ]  11/10/2009  Fixes to use of PU_CACHE have made this usable
 // It is slower because small blocks search from beginning, but that should
 // reduce fragmentation.
 // [WDJ]  1/22/2009  Fragmentation reduction.
 // This hangs FreeDoom MAP10 on the second or third reload.
 // A sure test is to start with -mb 35, then "MAP MAP10", "MAP MAP11",
 // "MAP MAP10", which will then hang in B_BuildNodes,
 // where it normally takes 12 seconds to load.
 // PU_CACHE is being used with other Z_Malloc calls, and allocated memory
 // is being purged before the user is done.  Using PU_CACHE on newly
 // allocated blocks only works if Z_ALLOC takes a long time to cycle back
 // around to them.  But when memory gets tight and it gets back to them
 // quicker, then we get mysterious failures.  Because a lump may already
 // be in memory, as a PU_CACHE tagged allocation, there is no assurance that
 // a call to Z_Malloc will not free the PU_CACHE memory being used.
    if( reqsize < 2048 ) {
       rover = &mainzone->blocklist;	// try to find freed block first
    }else{
       rover = mainzone->rover;
    }
#else
    // Older, faster, method of allocate all zone first, then cycle back through,
    // which tends to fragment memory and does not leave any large blocks free.
    rover = mainzone->rover;
#endif

    // if there is a free block preceding base (there can be at most one), move one block back
    if ( rover->prev->tag == PU_FREE )  // move back if prev is free
        rover = rover->prev;

    start = rover->prev;  // for test when have searched entire zone

    // Check on user code corrupting the memory block
    if (rover->id && rover->id != ZONEID) //Hurdler: this shouldn't happen
    {
        // [WDJ] 11/18/2009 Still get this error on some wads.  There must be
        // some unchecked use of memory that writes past end of allocation.
	// FIXME: Find the source of this error !!
        printf("WARNING: Legacy may crash soon. This is a known bug, sorry.\n");
        printf("Memory corruption has been detected\n");
        printf("Known to happen when node-builder is not run after editing level.\n");
    }

    for(;;)	// Search zone memory
    {
        if (rover == start)
        {
            // scanned all the way around the list
            //faB: debug to see if problems of memory fragmentation..
            Command_Memfree_f();
	   
	    tries ++;
	    if( tries < 4 ) {
#ifdef GROW_ZONE
	       // Grow the zone allocation, and alloc from new memory
	       rover = Z_GrowZone( reqsize, GROW_MAIN_MEM_MB<<20 );
	       if( rover == NULL ) {
		   // new allocation failed, try something desperate
		   rover = Z_GrowZone( reqsize, 0);
		   if( rover == NULL ) {
		       fprintf(stderr,"Z_Malloc: Retry %i on allocation of %i bytes\n",
			       tries, memalloc_size );
		       rover = start;
		   }
	       }
  	       base = NULL;
#else
	       fprintf(stderr,"Z_Malloc: Retry %i on allocation of %i bytes\n",
		       tries, memalloc_size );
#endif	       
	    }else{
	       I_Error ("Z_Malloc: failed on allocation of %i bytes\n"
                     "Try to increase heap size using -mb parameter (actual heap size : %d Mb)\n", memalloc_size, mb_used);
	    }
        }

        if (rover->tag != PU_FREE)		// being used
        {
            if (rover->tag < PU_PURGELEVEL)
            {
                // hit a block that can't be purged, so move past it

                //Hurdler: FIXME: this is where the crashing problem seem to come from
	        // [WDJ] 1/20/2009 Found other bugs that it probably interacted with.
                rover = rover->next;
	        base = NULL;	// cancel current consideration
	        basesize = 0;
	        continue;
            }
	    // Purgable block
#ifdef AGGRESSIVE_PURGE
            else
            {
                // free the rover block (adding the size to base)

                // the base cannot be rover, but could be rover->prev
	        memblock_t* roverprev = rover->prev;	// back away from block that can disappear
	        int roverprev_size = roverprev->size;	// size change test
                Z_Purge( rover );
	          // freed memory can be put in prev, or same block
	        // roverprev is unknown
	        rover = roverprev;
	        if( roverprev->size != roverprev_size ) {   // size changed
		    // roverprev was PU_FREE, and freed block was added to it
		    // old rover block is gone
		    // must redo roverprev because size changed
	            if( base ) {
		        // changed free block changes basesize
		        basesize -= roverprev_size;	// old size removed
		        // continue and let normal code add in new size
		    }
		    // add rover (roverprev) and test the existing base size
		}else{
		    // old rover block is still there, but now free
		    rover = roverprev->next;
		    // can now be considered same as existing free block
		}
            }
#endif
	}
        // rover is free or purgable
        // accumulate it as part of considered base
        if( ! base ) {
	   base = rover;	// start new base
	   basesize = 0;
	}
        basesize += rover->size;       // accmulate the size
        rover = rover->next;
       
        // base is available, so test it against size
       
	// trial data alignment
        basedata = ALIGN((ULONG)base + sizeof(memblock_t));
	//Hurdler: huh? it crashed on my system :( (with 777.wad and 777.deh, only software mode)
	//         same problem with MR.ROCKET's wad -> it's probably not a problem with the wad !?
	//         this is because base doesn't point to something valid (and it's not NULL)
	// Check addr of end of blocks for fit
	// 	if( ((ULONG)base)+base->size >= basedata+memalloc_size-sizeof(memblock_t) ) break;
	if( (((ULONG)base) + basesize) >= (basedata + reqsize) ) {
	   // fits
#if 0
	   // [WDJ] Attempt at better allocation, does not have any effect
	   if( tries == 0 ) {
	      // Try harder to not fragment memory
	      extra = basesize - reqsize;
	      if( (extra > 32) && (extra < memalloc_size) ) continue;
	   }
#endif
	   // [WDJ] 1/20/2009 * CRITICAL ERROR FIX *
	   if( alignbits ) {  // consider alignment problems
	      // [WDJ] More complete checking to avoid misalign problems later.
	      // If misalignment is not large enough for a MINFRAGMENT, then
	      // cannot always find a place to put it, and it will cause errors.
	      // Eliminate problem by selecting a different block.
              memblock_t*  newbase = ((memblock_t*)basedata) - 1;
	      misalign = (byte*)newbase - (byte*)base;	// extra before
	      if( misalign <= MINFRAGMENT ) {   // have a problem
		 // require room for MINFRAGMENT to hold misalign memory.
		 // with at least 1 byte in the fragment, to avoid a strange case
		 basedata = ALIGN( (ULONG)base + sizeof(memblock_t) + MINFRAGMENT + 1 );
		 if( (((ULONG)base) + basesize) >= (basedata + reqsize) )  break;  // OK
		 continue;	// too small for misalign, try another block
	      }
	   }
	   break;	// found block, break out of search loop
	}
    }

    // Free the purgable blocks under consideration, combining them into
    // one free block.
    // [WDJ] Fragile code, do not modify unless you are willing to spend
    // a few days testing for failures, after 3 level changes in certain wads,
    // with AGGRESSIVE_PURGE on and off, with GROW_ZONE on and off.
    Z_Purge( base );  // free the base
    if( base->prev->next != base ) {
        // base ought to be the first in any group of purgable, so this should not happen
        I_Error("Z_MALLOC: internal error, purged base disappeared");
    }
    // rover can combine with a free during the loop, or it could be
    // zone head (low address), so it is not reliable to test against rover.
    while( base->size < basesize )
    {
        // stop at rover, end of tested blocks
        if( base->next == rover )   break;
        if( base->next->tag == PU_ZONE )   break;  // failed, break tight loop
        // free the next block, combining it with base
	Z_Purge( base->next );
    }
    if( base->size < basesize ) {
        I_Error("Z_MALLOC: internal error, combined blocks less than request size");
    }

    // aligning can leave free space in current block so make it really free
    if( alignbits )
    {
        // The new, aligned, block.
	// Sub 1 from memblock ptr is same as sub of header size.
        memblock_t*  newbase = ((memblock_t*)basedata) - 1;
        misalign = (byte*)newbase - (byte*)base;	// extra before

	// [WDJ] 1/20/2009 loop ensures misalign is 0, or >= MINFRAGMENT.
        if( misalign > MINFRAGMENT )
        {
	    // MINFRAGMENT >= sizeof( memblock_t )
	    // so base header does not overlap newbase header
	    // Link in newbase after base, and change base size.
	    Z_LinkBlock( newbase, base, base->next );

            newbase->size = base->size - misalign;
            base->size = misalign;
        }
        else
        {
	    fprintf(stderr,"Z_ALLOC: internal error, misalign < MINFRAGMENT\n" );
        }
        base = newbase;	// aligned
    }

    // commit to using the free block

    // tag marks block as in use
    base->tag = tag;	// must be done before Z_CombineFreeBlock
    base->id = ZONEID;
    base->user = user;	// valid or NULL
#ifdef ZDEBUG
    base->ownerfile = file;
    base->ownerline = line;
#endif
    // how much too big
    int extra = base->size - memalloc_size;
    if (extra > MINFRAGMENT)
    {
        // there will be a free fragment after the allocated block
        memblock_t * exblock = (memblock_t *) ((byte *)base + memalloc_size );
        exblock->size = extra;
        base->size = memalloc_size;

        Z_LinkBlock(exblock, base, base->next);
        // non-free tag on base prevents it from combining with exblock!
        Z_CombineFreeBlock(exblock);
    }

    // pointer to the data area after header; aligned has base already aligned
    void * blockdata = (byte *)base + sizeof(memblock_t);

    if (user)
    {
        // setup user owner ptr
        *(void **)user = blockdata;
    }
    else
    {
	// [WDJ] Most of the calls have user==NULL !!
        if (tag >= PU_PURGELEVEL) {
            //I_SoftError ("Z_Malloc: an owner is required for purgable blocks");
	    tag = PU_LEVEL;	// does not require user
	}
    }

    // next allocation will start looking here
    mainzone->rover = base->next;

    return blockdata;
}
#endif   



//
// Z_FreeTags
//
void Z_FreeTags( memtag_e lowtag, memtag_e hightag )
{
#ifdef PLAIN_MALLOC
    // cannot search, no blocklist
    return;
#else
// TAGGED_MALLOC, ZONE_ZALLOC
    memblock_t* block;
    memblock_t* next;
   
    for (block = mainzone->blocklist.next ;
         block != &mainzone->blocklist ;
         block = next)
    {
        // get link before freeing
        next = block->next;

#ifdef TAGGED_MALLOC
        // does not have any blocks with PU_FREE
#else       
        if (block->tag == PU_FREE)   continue;
#endif       

        if (block->tag >= lowtag && block->tag <= hightag)
            Z_Free ( (byte *)block+sizeof(memblock_t));
    }
#endif
}


//
// Z_ChangeTags_To
// Change all blocks of old_tag to new_tag.
//
void Z_ChangeTags_To( memtag_e old_tag, memtag_e new_tag )
{
#ifdef PLAIN_MALLOC
    return;
#else
// TAGGED_MALLOC, ZONE_ZALLOC
    memblock_t* block;
    for (block = mainzone->blocklist.next ;
         block != &mainzone->blocklist ;
         block = block->next)
    {
        // free blocks will not match
        if (block->tag == old_tag)  block->tag = new_tag;
    }
#endif   
}


//
// Z_DumpHeap
// Note: TFileDumpHeap( stdout ) ?
//
void Z_DumpHeap(memtag_e lowtag, memtag_e hightag)
{
    memblock_t* block;

    CONS_Printf ("zone size: %li  location: %p\n",
            mainzone->size,mainzone);

    CONS_Printf ("tag range: %i to %i\n",
            lowtag, hightag);

    for (block = mainzone->blocklist.next ; ; block = block->next)
    {
        if (block->tag >= lowtag && block->tag <= hightag)
            CONS_Printf ("block:%p    size:%7i    user:%p    tag:%3i prev:%p next:%p\n",
                    block, block->size, block->user, block->tag, block->next, block->prev);

        if (block->next == &mainzone->blocklist)
        {
            // all blocks have been hit
            break;
        }
#ifdef GROW_ZONE
        if ( block->next->tag != PU_ZONE )	// exclude zone heads
#endif
        if ( (byte *)block + block->size != (byte *)block->next)
            CONS_Printf ("ERROR: block size does not touch the next block\n");

        if ( block->next->prev != block)
            CONS_Printf ("ERROR: next block doesn't have proper back link\n");

        if ( block->tag==PU_FREE && block->next->tag==PU_FREE )
            CONS_Printf ("ERROR: two consecutive free blocks\n");
    }
}


//
// Z_FileDumpHeap
//
void Z_FileDumpHeap (FILE* f)
{
    memblock_t* block;
    int i=0;

    fprintf (f, "zone size: %li     location: %p\n",mainzone->size,mainzone);

    for (block = mainzone->blocklist.next ; ; block = block->next)
    {
        i++;
        fprintf (f,"block:%p size:%7i user:%7x tag:%3i prev:%p next:%p id:%7i\n",
                 block, block->size, (int)block->user, block->tag, block->prev, block->next, block->id);

        if (block->next == &mainzone->blocklist)
        {
            // all blocks have been hit
            break;
        }

        if ( (block->user > (void **)0x100) && 
             ((int)(*(block->user))!=((int)block)+(int)sizeof(memblock_t)))
            fprintf (f,"ERROR: block don't have a proper user\n");

#ifdef GROW_ZONE
        if ( block->next->tag != PU_ZONE )	// exclude zone heads
#endif
        if ( (byte *)block + block->size != (byte *)block->next)
            fprintf (f,"ERROR: block size does not touch the next block\n");

        if ( block->next->prev != block)
            fprintf (f,"ERROR: next block doesn't have proper back link\n");

        if ( block->tag==PU_FREE && block->next->tag==PU_FREE )
            fprintf (f,"ERROR: two consecutive free blocks\n");
    }
    fprintf (f,"Total : %d blocks\n"
               "===============================================================================\n\n",i);
}



//
// Z_CheckHeap
//
void Z_CheckHeap (int i)
{
#ifdef PLAIN_MALLOC
    return;
#else
// TAGGED_MALLOC, ZONE_ZALLOC
    memblock_t* block;
    for (block = mainzone->blocklist.next ; ; block = block->next)
    {
        if (block->next == &mainzone->blocklist)
        {
            // all blocks have been hit
            break;
        }

        if ( (block->user > (void **)0x100) && 
             ((int)(*(block->user))!=((int)block)+(int)sizeof(memblock_t)))
            I_Error ("Z_CheckHeap: block don't have a proper user %d\n",i);

#ifdef ZONE_ZALLOC
#ifdef GROW_ZONE
        if ( block->next->tag != PU_ZONE )	// exclude zone heads
#endif
        if ( (byte *)block + block->size != (byte *)block->next)
            I_Error ("Z_CheckHeap: block size does not touch the next block %d\n",i);
#endif       
// TAGGED_MALLOC, ZONE_ZALLOC
        if ( block->next->prev != block)
            I_Error ("Z_CheckHeap: next block doesn't have proper back link %d\n",i);
#ifdef ZONE_ZALLOC
        if ( block->tag==PU_FREE && block->next->tag==PU_FREE )
            I_Error ("Z_CheckHeap: two consecutive free blocks %d\n",i);
#endif
    }
#endif	// TAGGED_MALLOC, ZONE_ZALLOC
}




//
// Z_ChangeTag
//
void Z_ChangeTag2 ( void* ptr, memtag_e tag )
{
#ifdef PLAIN_MALLOC
    return;
#else
// TAGGED_MALLOC, ZONE_ZALLOC
    memblock_t* block;
    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
        I_Error ("Z_ChangeTag: freed a pointer without ZONEID");

    if (tag >= PU_PURGELEVEL && (unsigned)block->user < 0x100)
        I_Error ("Z_ChangeTag: an owner is required for purgable blocks");

    if (tag == PU_FREE ) {
       fprintf(stderr,"Z_ChangeTag2 changing to 0 tag, conflict with FREE BLOCK\n" );
       tag = PU_LEVEL;	// safe
	// tag = PU_DAVE;	// if need to debug
    }

    // [WDJ] special tag changes which are conditional on existing tag
    switch( tag ) {
     case PU_CACHE_DEFAULT:
       // Change to PU_CACHE_DEFAULT is conditional.
       // Protect non-purgable lumps against casual degrading to PU_CACHE
       if (block->tag < PU_PURGELEVEL )  goto done;
       tag = PU_CACHE;
       break;
    
     case PU_LUMP:
       // PU_LUMP can become PU_CACHE, so do not override
       // a more restrictive tag
       if (block->tag < PU_LUMP )  goto done;
       break;
       
     case PU_IN_USE:
       // Becomes PU_CACHE later, so do not override a more restrictive tag
       if (block->tag < PU_IN_USE )  goto done;
       break;
     default:
       break;
    }


    block->tag = tag;

done:
    return;
#endif
}



//
// Z_FreeMemory
//
void Z_FreeMemory (int *realfree,int *cachemem,int *usedmem,int *largefreeblock)
{
#ifdef PLAIN_MALLOC
    *realfree = 0;
    *cachemem = 0;
    *usedmem = (int)memhead.size;
    *largefreeblock = 0;
    return;
#else
// ZONE_ZALLOC, TAGGED_MALLOC
    memblock_t*         block;
    int freeblock=0;

    *realfree = 0;
    *cachemem = 0;
    *usedmem  = 0;
    *largefreeblock = 0;

    for (block = mainzone->blocklist.next ;
         block != &mainzone->blocklist;
         block = block->next)
    {
        if ( block->tag == PU_FREE )
        {
            // free memory
            *realfree += block->size;
            freeblock += block->size;
            if(freeblock>*largefreeblock)
                *largefreeblock = freeblock;
        }
        else
        {
            if(block->tag >= PU_PURGELEVEL)
            {
                // purgable memory (cache)
                *cachemem += block->size;
                freeblock += block->size;
                if(freeblock>*largefreeblock)
                    *largefreeblock = freeblock;
            }
            else
            {
                // used block
                *usedmem += block->size;
                freeblock=0;
            }
	}
    }
#endif
}


//
// Z_TagUsage
// - return number of bytes currently allocated in the heap for the given tag
int Z_TagUsage (memtag_e tagnum)
{
    int             bytes = 0;

#if defined(TAGGED_MALLOC) || defined(ZONE_ZALLOC)
    memblock_t*     block;
    for (block = mainzone->blocklist.next ;
         block != &mainzone->blocklist;
         block = block->next)
    {
        if (block->tag == tagnum)
            bytes += block->size;
    }
#endif

    return bytes;
}


void Command_Memfree_f( void )
{
    ULONG freebytes, totalbytes;
#ifdef PLAIN_MALLOC
    CONS_Printf("\2Memory Heap Info - Plain Malloc\n");
    CONS_Printf("used  memory       : %7d kb\n", memhead.size>>10);
#else   
// ZONE_ZALLOC, TAGGED_MALLOC
    int   memfree,cache,used,largefreeblock;
   
    Z_CheckHeap (-1);
    Z_FreeMemory(&memfree,&cache,&used,&largefreeblock);
#ifdef TAGGED_MALLOC
    CONS_Printf("\2Memory Heap Info - Tagged Malloc\n");
    CONS_Printf("alloc memory       : %7d kb\n", memhead.size>>10);
#else
    CONS_Printf("\2Memory Heap Info\n");
    CONS_Printf("Total heap size    : %7d kb\n", mb_used<<10);
#endif   
    CONS_Printf("used  memory       : %7d kb\n", used>>10);
    CONS_Printf("free  memory       : %7d kb\n", memfree>>10);
    CONS_Printf("cache memory       : %7d kb\n", cache>>10);
    CONS_Printf("largest free block : %7d kb\n", largefreeblock>>10);
#ifdef HWRENDER
    if( rendermode != render_soft )
    {
    CONS_Printf("Patch info headers : %7d kb\n", Z_TagUsage(PU_HWRPATCHINFO)>>10);
    CONS_Printf("HW Texture cache   : %7d kb\n", Z_TagUsage(PU_HWRCACHE)>>10);
    CONS_Printf("Plane polygone     : %7d kb\n", Z_TagUsage(PU_HWRPLANE)>>10);
    CONS_Printf("HW Texture used    : %7d kb\n", HWD.pfnGetTextureUsed()>>10);
    }
#endif
#endif	// PLAIN_MALLOC

    CONS_Printf("\2System Memory Info\n");
    freebytes = I_GetFreeMem(&totalbytes);
    CONS_Printf("Total     physical memory: %6d kb\n", totalbytes>>10);
    CONS_Printf("Available physical memory: %6d kb\n", freebytes>>10);
}





char *Z_Strdup(const char *s, memtag_e tag, void **user)
{
  return strcpy(Z_Malloc(strlen(s)+1, tag, user), s);
}
