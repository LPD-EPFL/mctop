#include <helper.h>
#include <pfd.h>

int
set_cpu(int cpu) 
{
  int ret = 1;
#if defined(__sparc__)
  if (processor_bind(P_LWPID, P_MYID, cpu, NULL) == -1)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }
#elif defined(__tile__)
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, cpu)) < 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");
    }

  if (cpu != tmc_cpus_get_my_cpu())
    {
      PRINT("******* i am not CPU %d", tmc_cpus_get_my_cpu());
    }

#else
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }
#endif

  return ret;
}

int
get_num_hw_ctx()
{
  int nc = 0;
  while (1)
    {
      if (!set_cpu(nc))
  	{
  	  break;
  	}
      nc++;
    }
  return nc;
}

ticks
ll_random_traverse(volatile uint64_t* list, const size_t reps)
{
  volatile ticks __s = getticks();
  for (size_t r = 0; r < reps; r++)
    {
      list = (uint64_t*) *list;
    }
  volatile ticks __e = getticks();
  return (__e - __s) / reps;
}

void
ll_random_create(volatile uint64_t* mem, const size_t size)
{
  const size_t size_cl = size / CACHE_LINE_SIZE;
  const size_t per_cl = CACHE_LINE_SIZE / sizeof(uint64_t);
  uint8_t* used = calloc_assert(size_cl, sizeof(uint8_t));
  unsigned long* seeds = seeds_create();
  seeds[0] = 0x0123456789ABCDLL;
  seeds[1] = 0xA023457689BAC0LL;
  seeds[2] = 0xB0245736F9BAC1LL;

  size_t idx = 0;
  size_t used_num = 0;
  while (used_num < size_cl - 1)
    {
      used[idx] = 1;
      used_num++;

      size_t nxt;
      do
	{
	  nxt = (marsaglia_rand(seeds) % size_cl);
	}
      while (used[nxt]);

      size_t nxt_8 = (nxt * per_cl);
      size_t idx_8 = (idx * per_cl);
      mem[idx_8] = (uint64_t) (mem + nxt_8);
      idx = nxt;
    }

  mem[idx * per_cl] = (uint64_t) mem;
  /* mem[idx * per_cl] = 0; */

  free(seeds);
  free(used);
}

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
