#ifndef __H_MCTOP_INTERNAL__
#define __H_MCTOP_INTERNAL__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C" {
#endif

  /* ******************************************************************************** */
  /* MCTOP Cache */
  /* ******************************************************************************** */

  mctop_cache_info_t* mctop_cache_size_estimate();
  void mctop_cache_info_free(mctop_cache_info_t* mci);
  mctop_cache_info_t* mctop_cache_info_create(const uint n_levels);


  /* ******************************************************************************** */
  /* AUX functions */
  /* ******************************************************************************** */
  void** table_malloc(const size_t rows, const size_t cols, const size_t elem_size);
  void** table_calloc(const size_t rows, const size_t cols, const size_t elem_size);
  void table_free(void** m, const size_t cols);

  static inline void*
  malloc_assert(size_t size)
  {
    void* m = malloc(size);
    assert(m != NULL);
    return m;
  }

  static inline void*
  realloc_assert(void* old, size_t size)
  {
    void* m = realloc(old, size);
    assert(m != NULL);
    return m;
  }

  static inline void*
  calloc_assert(size_t n, size_t size)
  {
    void* m = calloc(n, size);
    assert(m != NULL);
    return m;
  }

  extern int mctop_set_cpu(int cpu);


#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_INTERNAL__ */

