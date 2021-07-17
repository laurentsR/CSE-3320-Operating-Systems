// Ryan Laurents - 1000763099
// CSE - 3320 [Operating Systems]
// Assignment 3 - Heap Management
// 11/13/2020

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// GRADER: Check lines 23-31 to see line numbers where stats are incremented
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)      ((b) + 1)
#define BLOCK_HEADER(ptr)   ((struct _block *)(ptr) - 1)


static int atexit_registered = 0;
static int num_mallocs       = 0; // Incremented on line 307
static int num_frees         = 0; // Incremented on line 348
static int num_reuses        = 0; // Incremented on line 300
static int num_grows         = 0; // Incremented on line 217
static int num_splits        = 0; // Incremented on line 267
static int num_coalesces     = 0; // Incremented on line 366 & 377
static int num_blocks        = 0; // Incremented on line 164
static int num_requested     = 0; // Summed on line 237
static int max_heap          = 0; // Summed on line 165

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit.  Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;         /* Size of the allocated _block of memory in bytes     */
   struct _block *prev;  /* Pointer to the previous _block of allocated memory   */
   struct _block *next;  /* Pointer to the next _block of allocated memory       */
   bool   free;          /* Is this _block free?                                */
   char   padding[3];
};

struct _block *lastUsed = NULL; // Used in Next Fit

struct _block *heapList = NULL; /* Free list to track the _blocks available */

/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 * \TODO Implement Next Fit
 * \TODO Implement Best Fit
 * \TODO Implement Worst Fit
 */
struct _block *findFreeBlock(struct _block **last, size_t size) 
{
   struct _block *curr = heapList;
   struct _block *temp = curr;

#if defined FIT && FIT == 0
   /* First fit */
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }
#endif

#if defined BEST && BEST == 0
   // We need to iterate through all our blocks. We check first for a block that is both free and larger than the size we need.
   // If we find one, we check the remaining size (current size - size needed) to see if it is the current SMALLEST remaining size.
   // If it is, we keep that size to compare to other blocks, and store that block away until we find another one that is smaller.
   // Once we have checked all blocks, we return the block with the smallest remaining size (leastRemaining).
   size_t leastRemaining = INT_MAX;
   struct _block *choice = NULL;
   while(curr)
   {
      *last = curr;
      if((curr->free) && (curr->size > size))
      {
         if((curr->size - size) < leastRemaining)
         {
            leastRemaining = curr->size - size;
            choice = curr;
         } 
      }
      curr = curr->next;
   }
   curr = choice;
#endif

#if defined WORST && WORST == 0
   // We need to iterate through all our blocks. We check first for a block that is both free and larger than the size we need.
   // If we find one, we check the remaining size (current size - size needed) to see if it is the current LARGEST remaining size.
   // If it is, we keep that size to compare to other blocks, and store that block away until we find another one that is larger.
   // Once we have checked all blocks, we return the block with the largest remaining size (mostRemaining).
   size_t mostRemaining = 0;
   struct _block *choice = NULL;
   while(curr)
   {
      *last = curr;
      if((curr->free) && (curr->size > size))
      {
         if((curr->size - size) > mostRemaining)
         {
            mostRemaining = curr->size - size;
            choice = curr;
         } 
      }
      curr = curr->next;
   }
   curr = choice;
#endif

#if defined NEXT && NEXT == 0
   // Next Fit is very similar to First Fit with one difference. FF will begin at the beginning of memory every time it needs to
   // malloc, while next fit will continue from the last point it left off. To achieve this, we will start with the base code from FF.
   // We will then add a global block struct to hold our last used block, this is defaulted to NULL. If this is NULL, meaning the first
   // time we malloc using NF, we will use the first fit algorithm. After finding the block, we will store that block in the global
   // variable named lastUsed so we can reference it next time. If lastUsed is not null, then we set curr = lastUsed so we can start 
   // the FF algorithm from that point.
   if(lastUsed)
   {
      curr = lastUsed;
   }
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }
   lastUsed = curr;
#endif

   // Count the blocks in the list and add the size of each block to the max heap
   while(temp)
   {
      num_blocks++;
      max_heap += (temp->size);
      temp = temp->next;
   }

   return curr;
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size) 
{
   /* Request more space from OS */
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   /* OS allocation failed */
   if (curr == (struct _block *)-1) 
   {
      return NULL;
   }

   /* Update heapList if not set */
   if (heapList == NULL) 
   {
      heapList = curr;
   }

   /* Attach new _block to prev _block */
   if (last) 
   {
      last->next = curr;
   }

   /* Update _block metadata */
   curr->size = size;
   curr->next = NULL;
   //curr->free = false;
   curr->prev = last;

   // Increment number of grows
   num_grows++;

   return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size) 
{
   // Add num requested here before size is aligned to a multiple of 4
   num_requested += (int)size;

   if( atexit_registered == 0 )
   {
      atexit_registered = 1;
      atexit( printStatistics );
   }

   /* Align to multiple of 4 */
   size = ALIGN4(size);

   /* Handle 0 size */
   if (size == 0) 
   {
      return NULL;
   }

   /* Look for free _block */
   struct _block *last = heapList;
   struct _block *next = findFreeBlock(&last, size);

   
   /* TODO: Split free _block if possible */
   if(next != NULL)
   {
      if(next->size > size)
      {
         // Create a helper variable for the current size and next for readability.
         // Create a new block to split into, update this block with the current size/prev etc. Update the next
         // block to show correct next/size/free.
         num_splits++;
         size_t oldSize = next->size;
         struct _block *oldNext = next;
         
         // Create new block 
         struct _block *split;
         split = (struct _block *)next;
         split->free = false;
         split->size = size;
         split->prev = next->prev;
         split->next = (struct _block*) split;
         
         // Update next block
         split->next->free = NULL;
         split->next->size = oldSize - split->size - sizeof(struct _block);
         split->next->next = oldNext;
         split->next->prev = split;
      }
   }

   /* Could not find free _block, so grow heap */
   if (next == NULL) 
   {
      next = growHeap(last, size);
   }

   /* Could not find free _block or grow heap, so just return NULL */
   if (next == NULL) 
   {
      return NULL;
   }

   if(next->free == true || next->free == false)
   {
      num_reuses++;
   }
   
   /* Mark _block as in use */
   next->free = false;

   // Increment number of mallocs
   num_mallocs++;

   /* Return data address associated with _block */
   return BLOCK_DATA(next);
}

void *realloc(void *oldPtr, size_t size)
{
   int *ptr = malloc(size);
   memcpy(ptr, oldPtr, size);
   return ptr;
}

void *calloc(size_t n, size_t size)
{
   int *ptr = malloc(n * size);
   memset(ptr, 0, n);
   return ptr;
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr) 
{

   if (ptr == NULL) 
   {
      return;
   }

   // Increment frees here. This will increment whenever free is called with a non NULL pointer
   num_frees++;

   /* Make _block as free */
   struct _block *curr = BLOCK_HEADER(ptr);
   assert(curr->free == 0);
   curr->free = true;
   
   /* TODO: Coalesce free _blocks if needed */
   // Here we check our newly free'd block if it has any free neighbors. We check next and prev
   // and if either are free, combine them. I'm unsure if we need to coelesce ALL free blocks
   // or just the ones we free, I currently have it set up for the latter. If we were to do all
   // free blocks, we would iterate through the list of blocks looking for free blocks. When we
   // find one, we can check to see if the next block is free as well. If it is, combine. Repeat
   // until you reach the end of the linked list. We increment the coelesces count in both ifs.
   if(curr->next)
   {
      if(curr->next->free != false)
      {
         num_coalesces++;
         
         curr->next = curr->next->next;
         curr->size = curr->size + curr->next->size;
      }
   }

   if(curr->prev)
   {
      if(curr->prev->free != false)
      {
         num_coalesces++;
         
         curr->prev->next = curr->next;
         curr->prev->size = curr->size + curr->prev->size;;
      }
   }
}

/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/
