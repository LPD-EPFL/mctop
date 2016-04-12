#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#if __x86_64__
#  include <numaif.h>
#endif

#include <tbb/parallel_sort.h>
#include "mctop_rand.h"
#include "mctop_sort.h"


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


static void
print_error_sorted(MCTOP_SORT_TYPE* array, const size_t n_elems, const uint print_always)
{
  uint sorted = 1;
  for (size_t i = 1; i < n_elems; i++)
    {
      if (array[i - 1] > array[i])
	{
	  printf(" >>> error: array[%zu] = %u > array[%zu] = %u\n",
		 i - 1, array[i - 1], i, array[i]);
	  sorted = 0;
	  break;
	}
    }
  if (print_always || !sorted)
    {
      printf("## Array %p {size %-10zu} is sorted: %u\n", array, n_elems, sorted);
    }
}

int
main(int argc, char **argv) 
{
  MCTOP_SORT_TYPE* array;
  size_t array_len = 256 * 1024 * 1024LL;
  /* const int array_max = 8192; */

  uint test_random_type = 0;

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
      c = getopt_long(argc, argv, "hm:n:p:r:s:g:i:", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) 
	{
	case 0:
	  /* Flag is automatically set */
	  break;
	case 'n':
	  break;
	case 's':
	  {
	    size_t array_mb = atol(optarg) * 1024 * 1024LU;
	    array_len = array_mb / sizeof(int);
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
        case 'p':
          break;
	default:
	  exit(1);
	}
    }

  unsigned long* seeds = seed_rand_fixed();

  const size_t array_siz = array_len * sizeof(MCTOP_SORT_TYPE);
  array = (MCTOP_SORT_TYPE*) malloc(array_siz);
  assert(array != NULL);

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
	  const uint j = mctop_rand(seeds) % array_len;
	  const MCTOP_SORT_TYPE tmp = array[i];
	  array[i] = array[j];
	  array[j] = tmp;
	}
      break;
    case 1:
      printf("  // Random 32bit \n");
      for (size_t i = 0; i < array_len; i++)
	{
	  array[i] = mctop_rand(seeds) % (2000000000);
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
      {
	printf("  // Already sorted - but array[0] <-> array[N-1] \n");
	for (size_t i = 0; i < array_len; i++)
	  {
	    array[i] = i;
	  }
	MCTOP_SORT_TYPE tmp = array[0];
	array[0] = array[array_len - 1];
	array[array_len - 1] = tmp;
      }
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

  printf("# Data = %llu MB \n", array_siz / (1024 * 1024LL));

  struct timespec start, stop;
  clock_gettime(CLOCK_REALTIME, &start);

  tbb::parallel_sort(array, array + array_len);

  clock_gettime(CLOCK_REALTIME, &stop);
  struct timespec dur = timespec_diff(start, stop);
  double dur_s = dur.tv_sec + (dur.tv_nsec / 1e9);
  printf("%s: ## Sorted %llu MB of ints in %f seconds\n", argv[0], array_siz / (1024 * 1024LL), dur_s);

  print_error_sorted(array, array_len, 0);

  free((void*) array);

  return 0;
}


