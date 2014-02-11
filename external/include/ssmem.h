/* December 10, 2013 */
#ifndef _SSMEM_H_
#define _SSMEM_H_

#include <stdio.h>
#include <stdint.h>


/* **************************************************************************************** */
/* parameters */
/* **************************************************************************************** */

#define SSMEM_GC_FREE_SET_SIZE 16380
#define SSMEM_DEFAULT_MEM_SIZE (32 * 1024 * 1024L)

/* **************************************************************************************** */
/* help definitions */
/* **************************************************************************************** */
#define ALIGNED(N) __attribute__ ((aligned (N)))
#define CACHE_LINE_SIZE 64

/* **************************************************************************************** */
/* data structures used by ssmem */
/* **************************************************************************************** */

/* a ssmem allocator */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_allocator
{
  union
  {
    struct
    {
      void* mem;		/* the actual memory the allocator uses */
      size_t mem_curr;		/* pointer to the next addrr to be allocated */
      size_t mem_size;		/* size of mem chunk */
      size_t tot_size;		/* total memory that the allocator uses */
      struct ssmem_list* mem_chunks; /* list of mem chunks (used to free the mem) */

      struct ssmem_ts* ts;	/* timestamp object associated with the allocator */

      struct ssmem_free_set* free_set_list; /* list of free_set. A free set holds freed mem 
					     that has not yet been reclaimed */
      size_t free_set_num;	/* number of sets in the free_set_list */
      struct ssmem_free_set* collected_set_list; /* list of collected_set. A collected set
						  contains mem that has been reclaimed */
      size_t collected_set_num;	/* number of sets in the collected_set_list */
      struct ssmem_free_set* available_set_list; /* list of set structs that are not used
						  and can be used as free sets */
    };
    uint8_t padding[2 * CACHE_LINE_SIZE];
  };
} ssmem_allocator_t;

/* a timestamp used by a thread */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_ts
{
  union
  {
    struct
    {
      size_t version;
      uint8_t id;
      struct ssmem_ts* next;
    };
  };
  uint8_t padding[CACHE_LINE_SIZE];
} ssmem_ts_t;

/* 
 * a timestamped free_set. It holds:  
 *  1. the collection of timestamps at the point when the free_set gets full
 *  2. the array of freed pointers to be used by ssmem_free()
 *  3. a set_next pointer in order to be able to create linked lists of
 *   free_sets
 */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_free_set
{
  size_t* ts_set;		/* set of timestamps for GC */
  size_t size;
  long int curr;		
  struct ssmem_free_set* set_next;
  uintptr_t* set;
} ssmem_free_set_t;

/*
 * a generic list that keeps track of actual memory that has been allocated
 * (using malloc / memalign) and the different allocators that the list is using
 */
typedef struct ssmem_list
{
  void* obj;
  struct ssmem_list* next;
} ssmem_list_t;

extern __thread volatile ssmem_ts_t* ssmem_ts_local;
extern __thread size_t ssmem_num_allocators;
extern __thread ssmem_list_t* ssmem_allocator_list;


/* **************************************************************************************** */
/* ssmem interface */
/* **************************************************************************************** */

/* initialize an allocator */
void ssmem_alloc_init(ssmem_allocator_t* a, size_t size, int id);
/* explicitely subscribe to the list of threads in order to used timestamps for GC */
void ssmem_gc_thread_init(ssmem_allocator_t* a, int id);
/* terminate the system (all allocators) and free all memory */
void ssmem_term();
/* terminate the allocator a and free all its memory
 * This function should NOT be used if the memory allocated by this allocator
 * might have been freed (and is still in use) by other allocators */
void ssmem_alloc_term(ssmem_allocator_t* a);

/* allocate some memory using allocator a */
inline void* ssmem_alloc(ssmem_allocator_t* a, size_t size);
/* free some memory using allocator a */
inline void ssmem_free(ssmem_allocator_t* a, void* obj);

/* debug/help functions */
void ssmem_ts_list_print();
size_t* ssmem_ts_set_collect();
void ssmem_ts_set_print(size_t* set);

void ssmem_free_list_print(ssmem_allocator_t* a);
void ssmem_collected_list_print(ssmem_allocator_t* a);
void ssmem_available_list_print(ssmem_allocator_t* a);
void ssmem_all_list_print(ssmem_allocator_t* a, int id);


/* **************************************************************************************** */
/* platform-specific definitions */
/* **************************************************************************************** */

#if defined(__x86_64__)
#  define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define FAI_U32(a) __sync_fetch_and_add(a,1)
#endif

#if defined(__sparc__)
#  include <atomic.h>
#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)
#  define FAI_U32(a) (atomic_inc_32_nv(a)-1)
#endif

#if defined(__tile__)
#  include <arch/atomic.h>
#  include <arch/cycle.h>
#  define CAS_U64(a,b,c) arch_atomic_val_compare_and_exchange(a,b,c)
#  define FAI_U32(a) arch_atomic_increment(a)
#endif


#endif /* _SSMEM_H_ */
