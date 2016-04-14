#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#if __x86_64__
#  include <numaif.h>
#endif

/* #define SORT_NAME int */
/* #define SORT_TYPE int */
/* #define SORT_CMP(x, y) ((x) - (y)) */
/* #include "sort.h" */

#include <mqsort.h>
#include <mmergesort.h>


#define mrand(x) xorshf96(&x[0], &x[1], &x[2])

static inline unsigned long* 
seed_rand() 
{
  unsigned long* seeds;
  seeds = (unsigned long*) malloc(64);
  seeds[0] = 11233311;
  seeds[1] = 2123123;
  seeds[2] = 313131222;
  return seeds;
}

//Marsaglia's xorshf generator
static inline unsigned long
xorshf96(unsigned long* x, unsigned long* y, unsigned long* z)  //period 2^96-1
{
  unsigned long t;
  (*x) ^= (*x) << 16;
  (*x) ^= (*x) >> 5;
  (*x) ^= (*x) << 1;

  t = *x;
  (*x) = *y;
  (*y) = *z;
  (*z) = t ^ (*x) ^ (*y);

  return *z;
}

struct timespec
timespec_diff(struct timespec start, struct timespec end)
{
  struct timespec temp;
  if ((end.tv_nsec-start.tv_nsec) < 0)
    {
      temp.tv_sec = end.tv_sec-start.tv_sec-1;
      temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    }
  else
    {
      temp.tv_sec = end.tv_sec-start.tv_sec;
      temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
  return temp;
}

int
main(int argc, char **argv) 
{
  uint* __attribute__((aligned(64))) array;
  int* chunks, chunk_size;
  uint chunks_per_thread = 1;
  size_t array_len = 1 * 1024 * 1024LL;
  uint test_random_type = 0;
  uint n_chunks = 1;


  struct option long_options[] = 
    {
      // These options don't set a flag
      {"help",                      no_argument,             NULL, 'h'},
      {"mct",                       required_argument,       NULL, 'm'},
      {NULL, 0, NULL, 0}
    };

  int i;
  char c;
  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hm:n:p:c:r:s:g:i:k:", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) 
	{
	case 0:
	  /* Flag is automatically set */
	  break;
	case 'c':
	  n_chunks = atoi(optarg);
	  break;
	case 's':
	  {
	    uint array_mb = atol(optarg) * 1024 * 1024LU;
	    array_len = array_mb / sizeof(int);
	  }
	  break;
	case 'k':
	  {
	    //uint array_kb = atol(optarg) * 1024LU;
	    //	    array_len = array_kb / sizeof(int);
	    array_len = atol(optarg);
	  }
	  break;
	case 'r':
	  test_random_type = atoi(optarg);
	  break;
	case 'h':
	  mctop_alloc_help();
	  exit(0);
	case '?':
	  printf("Use -h or --help for help\n");
	  exit(0);
	default:
	  exit(1);
	}
    }

  unsigned long* seeds = seed_rand();

  const size_t array_siz = array_len * sizeof(int);
  int ret = posix_memalign((void**) &array, 64, array_siz);
  assert(!ret && array != NULL);

  chunk_size = array_len / n_chunks;

  chunks = malloc(n_chunks * chunks_per_thread);
  assert(chunks != NULL);

  for (int i = 0; i < n_chunks; i++)
    {
      chunks[i] = i * chunk_size;
    }

  switch (test_random_type)
    {
    case 0:
      printf("  // Knuth shuffle \n");
      for (size_t i = 0; i < array_len; i++)
	{
	  array[i] = i;
	}
      for (size_t i = array_len - 1; i > 0; i--)
	{
	  const uint j = mrand(seeds) % array_len;
	  const int tmp = array[i];
	  array[i] = array[j];
	  array[j] = tmp;
	}
      break;
    case 1:
      printf("  // Random 32bit \n");
      for (size_t i = 0; i < array_len; i++)
	{
	  array[i] = mrand(seeds) % INT_MAX;
	}
      break;
    case 2:
      printf("  // Already sorted \n");
      for (size_t i = 0; i < array_len; i++)
	{
	  array[i] = i;
	}
      break;
    case 3:
      printf("  // Already sorted - but array[0] <-> array[N-1] \n");
      for (size_t i = 0; i < array_len; i++)
	{
	  array[i] = i;
	}
      int tmp = array[0];
      array[0] = array[array_len - 1];
      array[array_len - 1] = tmp;
      break;
    case 4:
      printf("  // Reverse sorted \n");
      for (size_t i = 0; i < array_len; i++)
	{
	  array[array_len - 1 - i] = i;
	}
      break;
    }
      

  free(seeds);

  printf("# Data = %llu MB -- #Chunks = %d -- Per chunk = %lu MB\n",
	 array_siz / (1024 * 1024LL), n_chunks, (chunk_size * sizeof(int)) / (1024 * 1024));

  struct timespec start, stop;
  clock_gettime(CLOCK_REALTIME, &start);
  
  for (int i = 0; i < n_chunks; i++)
    {
      mqsort(array + chunks[i], chunk_size);
      /* mmergesort(array + chunks[i], chunk_size); */
    }

  clock_gettime(CLOCK_REALTIME, &stop);
  struct timespec dur = timespec_diff(start, stop);
  double dur_s = dur.tv_sec + (dur.tv_nsec / 1e9);
  printf("## Sorted %llu MB of ints in %f seconds\n", array_siz / (1024 * 1024LL), dur_s);

  for (int c = 0; c < n_chunks; c++)
    {
      uint* a = array + chunks[c];
      for (uint i = 0; i < chunk_size - 1; i++)
	{
	  /* assert(array_out[i] < array_out[i + 1]); */
	  if (a[i]  > a[i + 1])
	    {
	      printf("array_out[%d] = %-5d > array_out[%d] = %-5d\n",
		     i, a[i], i + 1, a[i + 1]);
	      break;
	    }
	}
    }


  free((void*) array);
  free(chunks);
  return 0;
}
