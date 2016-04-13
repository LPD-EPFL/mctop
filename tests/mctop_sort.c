#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#if __x86_64__
#  include <numaif.h>
#endif

#include <mctop_rand.h>
#include <mctop_sort.h>


void* test_pin(void* params);

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

int is_power_of_two (uint x)
{
   while (((x % 2) == 0) && x > 1) /* While x is even and > 1 */
        x /= 2;
    return (x == 1);
}

int
main(int argc, char **argv) 
{
  MCTOP_SORT_TYPE* array;
  size_t array_len = 256 * 1024 * 1024LL;
  /* const int array_max = 8192; */

  char mct_file[100];
  uint manual_file = 0;
  int test_num_threads = 2;
  int test_num_hwcs_per_socket = MCTOP_ALLOC_ALL;
  mctop_alloc_policy test_policy = MCTOP_ALLOC_SEQUENTIAL;
  uint test_random_type = 0;
  uint test_verbose = 0;

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
      c = getopt_long(argc, argv, "hm:n:p:c:r:s:g:i:v", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) 
	{
	case 0:
	  /* Flag is automatically set */
	  break;
	case 'm':
	  sprintf(mct_file, "%s", optarg);
	  manual_file = 1;
	  break;
	case 'n':
	  test_num_threads = atoi(optarg);
	  break;
	case 'c':
	  test_num_hwcs_per_socket = atoi(optarg);
	  break;
	case 'p':
	  test_policy = (mctop_alloc_policy) atoi(optarg);
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
	case 'v':
	  test_verbose = 1;
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

  mctop_t* topo;
  if (manual_file)
    {
      topo = mctop_load(mct_file);
    }
  else
    {
      topo = mctop_load(NULL);
    }

  if (topo)
    {
      //      mctop_print(topo);
      mctop_alloc_t* alloc = mctop_alloc_create(topo, test_num_threads, test_num_hwcs_per_socket, test_policy);
      /* mctop_alloc_print(alloc); */
      mctop_alloc_print_short(alloc);
#if MCTOP_SORT_USE_SSE == 1
      mctop_node_tree_t* nt = mctop_alloc_node_tree_create(alloc, CORE);
#elif MCTOP_SORT_USE_SSE == 2
      mctop_node_tree_t* nt = mctop_alloc_node_tree_create(alloc, CORE);
#elif MCTOP_SORT_USE_SSE == 0
      mctop_node_tree_t* nt = mctop_alloc_node_tree_create(alloc, HW_CONTEXT);
#elif MCTOP_SORT_USE_SSE == 3
      mctop_node_tree_t* nt = mctop_alloc_node_tree_create(alloc, EVERYONE);
#endif
      if (test_verbose)
	{
	  mctop_node_tree_print(nt);
	}

      const uint fnode = mctop_node_tree_get_final_dest_node(nt);
      mctop_alloc_pin_nth_socket(alloc, fnode);

      unsigned long* seeds = seed_rand_fixed();

      const size_t array_siz = array_len * sizeof(MCTOP_SORT_TYPE);
      array = (MCTOP_SORT_TYPE*) malloc(array_siz);
      assert(array != NULL);

      if (!is_power_of_two(mctop_alloc_get_num_sockets(alloc))) {
        printf("%s: ## Sorted %llu MB of ints in %f seconds\n", argv[0], array_siz / (1024 * 1024LL), 0.0);
        return 0;
      }

      switch (test_random_type)
	{
	case 0:
	  printf("  // Knuth shuffle \n");
          mctop_get_knuth_array_uint(array, array_len, seeds);
	  break;
	case 1:
	  printf("  // Random 32bit \n");
          mctop_get_random_array_uint(array, array_len, seeds);
	  break;
	case 2:
	  printf("  // Already sorted \n");
          mctop_get_sorted_array_uint(array, array_len);
	  break;
	case 3:
	  {
	    printf("  // Already sorted - but array[0] <-> array[N-1] \n");
            mctop_get_sorted_endswitched_array_uint(array, array_len);
	  }
	  break;
	case 4:
	  printf("  // Reverse sorted \n");
          mctop_get_reversesorted_array_uint(array, array_len);
	  break;
	}
      
      free(seeds);

      printf("# Data = %llu MB \n", array_siz / (1024 * 1024LL));

      struct timespec start, stop;
      clock_gettime(CLOCK_REALTIME, &start);
      mctop_sort(array, array_siz / sizeof(uint), nt);
      clock_gettime(CLOCK_REALTIME, &stop);
      struct timespec dur = timespec_diff(start, stop);
      double dur_s = dur.tv_sec + (dur.tv_nsec / 1e9);
      printf("%s: ## Sorted %llu MB of ints in %f seconds\n", argv[0], array_siz / (1024 * 1024LL), dur_s);

      print_error_sorted(array, array_len, 0);
      mctop_alloc_free(alloc);
      mctop_node_tree_free(nt);

      mctop_free(topo);
      free((void*) array);
    }
  return 0;
}


