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

#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_INTERNAL__ */

