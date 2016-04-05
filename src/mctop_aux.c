#include <mctop.h>
#include <mctop_internal.h>

void**
table_malloc(const size_t rows, const size_t cols, const size_t elem_size)
{
  void** m = malloc_assert(rows * sizeof(uint64_t*));
  for (int s = 0; s < rows; s++)
    {
      m[s] = malloc_assert(cols * elem_size);
    }

  return m;
}

void**
table_calloc(const size_t rows, const size_t cols, const size_t elem_size)
{
  void** m = malloc_assert(rows * sizeof(uint64_t*));
  for (int s = 0; s < rows; s++)
    {
      m[s] = calloc_assert(cols, elem_size);
    }

  return m;
}

void
table_free(void** m, const size_t cols)
{
  for (int s = 0; s < cols; s++)
    {
      free(m[s]);
    }
  free(m);
  m = NULL;
}
