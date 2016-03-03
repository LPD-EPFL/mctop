#ifndef __H_ATOMICS__
#define __H_ATOMICS__

#include <inttypes.h>

#ifdef __sparc__		/* SPARC */
#  include <atomic.h>

#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)

#  define FAI_U32(a) (atomic_inc_32_nv(a) - 1)
#  define IAF_U32(a) atomic_inc_32_nv((volatile uint32_t*) a)
#  define DAF_U32(a) atomic_dec_32_nv((volatile uint32_t*) a)
#  define IAF_U64(a) atomic_inc_64_nv(a)
#  define DAF_U64(a) atomic_dec_64_nv(a)

#  define MFENCE()   __asm volatile("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore"); 
#  define PAUSE()    __asm volatile("rd    %%ccr, %%g0\n\t" ::: "memory")
#elif defined(__tile__)		/* TILER */
#  include <arch/atomic.h>
#  include <arch/cycle.h>

#  define CAS_U64(a,b,c) arch_atomic_val_compare_and_exchange(a,b,c)

#  define FAI_U32(a) arch_atomic_increment(a)
#  define IAF_U32(a) (arch_atomic_increment(a) + 1)
#  define DAF_U32(a) (arch_atomic_decrement(a) - 1)
#  define IAF_U64(a) (arch_atomic_increment(a) + 1)
#  define DAF_U64(a) (arch_atomic_decrement(a) - 1)

#  define MFENCE()   arch_atomic_full_barrier()
#  define PAUSE()    cycle_relax()
#elif __x86_64__

#  define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)

#  define FAI_U32(a) __sync_fetch_and_add(a, 1)
#  define IAF_U32(a) __sync_add_and_fetch(a, 1)
#  define DAF_U32(a) __sync_sub_and_fetch(a, 1)
#  define IAF_U64(a) __sync_add_and_fetch(a, 1)
#  define DAF_U64(a) __sync_sub_and_fetch(a, 1)

#  define MFENCE()   __asm volatile ("mfence");
#  define PAUSE()    __asm volatile ("pause")


#else
#  error "Unsupported Architecture"
#endif

#endif	/* __H_ATOMICS__ */



