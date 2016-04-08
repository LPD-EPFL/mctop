#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#if __x86_64__
#  include <numaif.h>
#endif

#include <algorithm>    // std::sort

void* test_pin(void* params);

int* array, * array_out, * chunks;
size_t chunk_size = 1024 * 1024;
size_t n_chunks;

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
  size_t array_len = 256 * 1024 * 1024LL;
  /* const int array_max = 8192; */

  char mct_file[100];
  uint manual_file = 0;
  int test_num_threads = 2;
  int test_num_hwcs_per_socket = MCTOP_ALLOC_ALL;
  mctop_alloc_policy test_policy = MCTOP_ALLOC_SEQUENTIAL;
  const uint test_run_pin = 1;
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
	case 'g':
	  chunk_size = atoi(optarg) *  1024LU;
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

      unsigned long* seeds = seed_rand();

      const size_t array_siz = array_len * sizeof(int);
      array = (int*) malloc(array_siz);
      assert(array != NULL);
      array_out = (int*) malloc(array_siz);
      assert(array_out != NULL);

      n_chunks = array_siz / chunk_size;

      chunks = (int*) malloc(n_chunks * sizeof(int));
      chunk_size = array_len / n_chunks;

      for (uint i = 0; i < n_chunks; i++)
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

      printf("# Data = %llu MB -- #Chunks = %zu -- Per chunk = %lu MB\n",
	     array_siz / (1024 * 1024LL), n_chunks, (chunk_size * sizeof(int)) / (1024 * 1024));

      if (test_run_pin)
	{
	  const uint n_hwcs = mctop_alloc_get_num_hw_contexts(alloc);
	  pthread_t threads[n_hwcs];
	  pthread_attr_t attr;
	  void* status;
    
	  /* Initialize and set thread detached attribute */
	  pthread_attr_init(&attr);
	  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
	  struct timespec start, stop;
	  clock_gettime(CLOCK_REALTIME, &start);

	  for(uint t = 0; t < n_hwcs; t++)
	    {
	      int rc = pthread_create(&threads[t], &attr, test_pin, alloc);
	      if (rc)
		{
		  printf("ERROR; return code from pthread_create() is %d\n", rc);
		  exit(-1);
		}
	    }

	  pthread_attr_destroy(&attr);

	  for(uint t = 0; t < n_hwcs; t++)
	    {
	      int rc = pthread_join(threads[t], &status);
	      if (rc) 
		{
		  printf("ERROR; return code from pthread_join() is %d\n", rc);
		  exit(-1);
		}
	    }

	  clock_gettime(CLOCK_REALTIME, &stop);
	  struct timespec dur = timespec_diff(start, stop);
	  double dur_s = dur.tv_sec + (dur.tv_nsec / 1e9);
	  printf("## Sorted %llu MB of ints in %f seconds\n", array_siz / (1024 * 1024LL), dur_s);

	  const size_t chunk_size_elems = chunk_size / sizeof(int);
	  for (uint c = 0; c < n_chunks; c++)
	    {
	      int* check = array_out + (c * chunk_size_elems);
	      for (uint i = 0; i < chunk_size_elems - 1; i++)
		{
		  if (check[i] > check[i + 1])
		    {
		      printf("check[%d] = %-5d > check[%d] = %-5d\n",
			     i, check[i], i + 1, check[i + 1]);
		      break;
		    }
		}
	    }
	}

      mctop_alloc_free(alloc);

      mctop_free(topo);
      free((void*) array);
      free(chunks);
    }
  return 0;
}


int
cmpfunc(const void* a, const void* b)
{
  return (*(int*)a - *(int*)b);
}

void*
test_pin(void* params)
{
  mctop_alloc_t* alloc = (mctop_alloc_t*) params;
  mctop_alloc_pin(alloc);

  const uint id = mctop_alloc_thread_id();
  const uint n_threads =mctop_alloc_get_num_hw_contexts(alloc);

  const size_t chunk_size_b = chunk_size * sizeof(int);
  const size_t remain = n_chunks %  n_threads;
  const size_t remain_per_sock = remain / mctop_alloc_get_num_sockets(alloc);
  const size_t extra = (id < remain_per_sock);

  const size_t n_chunks_mine = n_chunks / mctop_alloc_get_num_hw_contexts(alloc) + extra;
  
  for (uint c = 0; c < n_chunks_mine; c++)
    {
      int* a = array + (((c * n_threads) + id) * chunk_size);
      int* bt = array_out + (((c * n_threads) + id) * chunk_size);
      memcpy(bt, a, chunk_size_b);
      std::sort(bt, bt + chunk_size);
      // std::sort_heap(start, stop);
      /* free(b); */
      /* qsort(a, chunk_size, sizeof(int), cmpfunc); */
    }

  return NULL;
}
