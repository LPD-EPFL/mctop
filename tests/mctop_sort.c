#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#if __x86_64__
#  include <numaif.h>
#endif

#include <mctop_sort.h>

#include <mqsort.h>
#include <mmergesort.h>

/* #define SORT_NAME int */
/* #define SORT_TYPE int */
/* #define SORT_CMP(x, y) ((x) - (y)) */
/* #include "sort.h" */

void* test_pin(void* params);

#define mrand(x) xorshf96(&x[0], &x[1], &x[2])

static inline unsigned long* 
seed_rand() 
{
  unsigned long* seeds;
  seeds = (unsigned long*) malloc(64);
  seeds[0] = 1;
  seeds[1] = 2;
  seeds[2] = 5;
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

#ifdef __x86_64__
int
get_numa_node(void* mem, const uint n_nodes)
{
  int numa_node = -1;
  get_mempolicy(&numa_node, NULL, 0, (void*)mem, MPOL_F_NODE | MPOL_F_ADDR);
  return numa_node;
}
#elif __sparc
static uint cnode = 0;
int
get_numa_node(void* mem, const uint n_nodes)
{
  return cnode++ % n_nodes;
}
#endif

typedef struct wq_data
{
  uint interleaved;
  uint sorted;
  size_t len;
  int* array;
} wq_data_t;


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
      c = getopt_long(argc, argv, "hm:n:p:c:r:s:g:i:", long_options, &i);

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
	    uint array_mb = atol(optarg) * 1024 * 1024LU;
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
      mctop_node_tree_t* nt = mctop_alloc_node_tree_create(alloc, CORE);

      unsigned long* seeds = seed_rand();

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
	      const uint j = rand() % array_len; //mrand(seeds) % array_len;
	      const MCTOP_SORT_TYPE tmp = array[i];
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
	  {
	    printf("  // Already sorted - but array[0] <-> array[N-1] \n");
	    for (size_t i = 0; i < array_len; i++)
	      {
		array[i] = i;
	      }
	    int tmp = array[0];
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
      mctop_sort(array, array_siz / sizeof(uint), nt);
      clock_gettime(CLOCK_REALTIME, &stop);
      struct timespec dur = timespec_diff(start, stop);
      double dur_s = dur.tv_sec + (dur.tv_nsec / 1e9);
      printf("## Sorted %llu MB of ints in %f seconds\n", array_siz / (1024 * 1024LL), dur_s);

      for (uint c = 0; c < array_len - 1; c++)
	{
	  if (array[c] > array[c + 1])
	    {
	      printf("array[%d] = %-5d > array[%d] = %-5d\n",
		     c, array[i], c + 1, array[c + 1]);
	      break;
	    }
	}
      mctop_alloc_free(alloc);
      mctop_node_tree_free(nt);

      mctop_free(topo);
      free((void*) array);
    }
  return 0;
}


int
cmpfunc(const void* a, const void* b)
{
  return (*(MCTOP_SORT_TYPE*)a - *(MCTOP_SORT_TYPE*)b);
}


volatile int remain_consumed = 0;

void*
test_pin(void* params)
{
  /* mctop_alloc_t* alloc = (mctop_alloc_t*) params; */
  /* mctop_alloc_pin(alloc); */

  /* const uint id = mctop_alloc_thread_id(); */
  /* const uint n_threads =mctop_alloc_get_num_hw_contexts(alloc); */

  /* const size_t chunk_size_b = chunk_size * sizeof(int); */
  /* const size_t remain = n_chunks %  n_threads; */

  /* /\* const size_t extra = 0; *\/ */
  /* /\* const size_t remain_per_sock = remain / mctop_alloc_get_num_sockets(alloc); *\/ */


  /* const size_t extra = mctop_alloc_thread_incore_id(); */
  /* const size_t n_chunks_mine = n_chunks / mctop_alloc_get_num_hw_contexts(alloc); */
  
  /* //  int* b = malloc(chunk_size_b * n_chunks_mine); */

  /* for (int c = 0; c < n_chunks_mine; c++) */
  /*   { */
  /*     int* a = array + (((c * n_threads) + id) * chunk_size); */
  /*     int* b = array_out + (((c * n_threads) + id) * chunk_size); */
  /*     memcpy(b, a, chunk_size_b); */
  /*     mqsort(b, chunk_size); */
  /*     /\* qsort(bt, chunk_size, sizeof(int), cmpfunc); *\/ */

  /*     /\* qsort(bt, chunk_size, sizeof(int), cmpfunc); *\/ */
  /*     /\* int_merge_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* mmergesort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_selection_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_binary_insertion_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_heap_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_shell_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_tim_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_merge_sort_in_place(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_grail_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_sqrt_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_rec_stable_sort(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* int_grail_sort_dyn_buffer(b + (c * chunk_size), chunk_size); *\/ */
  /*     /\* free(b); *\/ */
  /*     /\* qsort(a, chunk_size, sizeof(int), cmpfunc); *\/ */
  /*   } */
  
  /* if (extra) */
  /*   { */
  /*     do */
  /* 	{ */
  /* 	  int nc = __sync_fetch_and_add(&remain_consumed, 1); */
  /* 	  if (nc < remain) */
  /* 	    { */
  /* 	      printf("%u -> ht, wooork - %d!\n", id, nc); */
  /* 	      int* a = array + (n_threads * n_chunks_mine * chunk_size) + (nc * chunk_size); */
  /* 	      int* b = array_out + (n_threads * n_chunks_mine * chunk_size) + (nc * chunk_size); */
  /* 	      memcpy(b, a, chunk_size_b); */
  /* 	      mqsort(b, chunk_size); */
  /* 	    } */
  /* 	  else */
  /* 	    { */
  /* 	      break; */
  /* 	    } */
  /* 	} */
  /*     while (1); */
  /*   } */
  return NULL;
}