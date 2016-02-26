#ifndef __H_ATOMICS__
#define __H_ATOMICS__

#include <inttypes.h>

#ifdef __sparc__		/* SPARC */
#  include <atomic.h>

#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)

#  define IAF_U32(a) atomic_inc_32_nv(a)
#  define DAF_U32(a) atomic_dec_32_nv(a)
#  define IAF_U64(a) atomic_inc_64_nv(a)
#  define DAF_U64(a) atomic_dec_64_nv(a)

#elif defined(__tile__)		/* TILER */
#  include <arch/atomic.h>
#  include <arch/cycle.h>

#  define CAS_U64(a,b,c) arch_atomic_val_compare_and_exchange(a,b,c)

#  define IAF_U32(a) (arch_atomic_increment(a)+1)
#  define DAF_U32(a) (arch_atomic_decrement(a)-1)
#  define IAF_U64(a) (arch_atomic_increment(a)+1)
#  define DAF_U64(a) (arch_atomic_decrement(a)-1)

#else  /* X86 */

#  define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)

#  define IAF_U32(a) __sync_add_and_fetch(a,1)
#  define DAF_U32(a) __sync_sub_and_fetch(a,1)
#  define IAF_U64(a) __sync_add_and_fetch(a,1)
#  define DAF_U64(a) __sync_sub_and_fetch(a,1)

#define PAUSE() __asm volatile ("pause")
#endif


#endif	/* __H_ATOMICS__ */



