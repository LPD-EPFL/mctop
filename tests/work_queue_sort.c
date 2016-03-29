#include <mctop.h>
#include <pthread.h>
#include <getopt.h>
#include <numaif.h>

void* test_pin(void* params);

volatile int* array, * array_out;

#define mrand(x) xorshf96(&x[0], &x[1], &x[2])

static inline unsigned long* 
seed_rand() 
{
  unsigned long* seeds;
  seeds = (unsigned long*) malloc(64);
  seeds[0] = 1;
  seeds[1] = 2;
  seeds[2] = 3;
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

int get_numa_node(void* mem)
{
  int numa_node = -1;
  get_mempolicy(&numa_node, NULL, 0, (void*)mem, MPOL_F_NODE | MPOL_F_ADDR);
  return numa_node;
}

typedef struct wq_data
{
  uint interleaved;
  uint sorted;
  size_t len;
  int* array;
} wq_data_t;

static inline wq_data_t*
wq_data_create(const uint interl, const uint sorted, const size_t len, int* array)
{
  wq_data_t* wqd = malloc(sizeof(wq_data_t));      
  wqd->interleaved = interl;
  wqd->sorted = sorted;
  wqd->len = len;
  wqd->array = array;
  return wqd;
}

int
main(int argc, char **argv) 
{
  size_t array_len = 128 * 1024 * 1024LL;
  const size_t array_max = 8192;

  char mct_file[100];
  uint manual_file = 0;
  int test_num_threads = 2;
  int test_num_hwcs_per_socket = MCTOP_ALLOC_ALL;
  mctop_alloc_policy test_policy = 1;
  uint test_run_pin = 0;

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
      c = getopt_long(argc, argv, "hm:n:p:c:rs:", long_options, &i);

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
	  test_policy = atoi(optarg);
	  break;
	case 's':
	  {
	    uint array_mb = atol(optarg) * 1024 * 1024LU;
	    array_len = array_mb / sizeof(int);
	  }
	  break;
	case 'r':
	  test_run_pin = 1;
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
      mctop_print(topo);

      mctop_alloc_t* alloc = mctop_alloc_create(topo, test_num_threads, test_num_hwcs_per_socket, test_policy);
      mctop_alloc_print(alloc);
      mctop_alloc_print_short(alloc);

      mctop_wq_t* wq = mctop_wq_create(alloc);

      struct bitmask* nodemask = mctop_alloc_create_nodemask(alloc);

      unsigned long* seeds = seed_rand();

      const size_t array_siz = array_len * sizeof(int);
      array = numa_alloc_interleaved_subset(array_siz, nodemask);
      assert(array != NULL);

      for (size_t i = 0; i < array_len; i++)
      	{
      	  array[i] = mrand(seeds) % array_max;
      	}

      free(seeds);

      const int page_size = getpagesize();
      const size_t array_len_pages = array_siz / page_size;
      const uint per_page = page_size / sizeof(uint);


      void* arrayv = (void*) array;
      int node_prev = get_numa_node(arrayv), n_pages = 0, chunk_size = -1;
      for (uint p = 0; p < array_len_pages; p++)
	{
	  n_pages++;
	  int node = get_numa_node(arrayv);
	  if (node != node_prev && chunk_size < 0)
	    {
	      chunk_size = n_pages * page_size;
	    }
	  /* printf("%p on %d\n", arrayv, node); */
	  wq_data_t* wqd = wq_data_create(1, 0, per_page, arrayv);
	  mctop_wq_enqueue_node(wq, node, wqd);
	  arrayv += page_size;
	  
	}

      mctop_wq_print(wq);
      printf("# Data = %llu MB (chunk size = %d = %llu MB)\n",
	     array_siz / (1024 * 1024LL),
	     chunk_size, chunk_size / (1024 * 1024LL));

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

	  for(int t = 0; t < n_hwcs; t++)
	    {
	      int rc = pthread_create(&threads[t], &attr, test_pin, wq);
	      if (rc)
		{
		  printf("ERROR; return code from pthread_create() is %d\n", rc);
		  exit(-1);
		}
	    }

	  pthread_attr_destroy(&attr);

	  for(int t = 0; t < n_hwcs; t++)
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

	  for (uint i = 0; i < array_len - 1; i++)
	    {
	      /* assert(array_out[i] < array_out[i + 1]); */
	      if (array_out[i]  > array_out[i + 1])
		{
		  printf("array_out[%d] = %-5d > array_out[%d] = %-5d\n",
			 i, array_out[i], i + 1, array_out[i + 1]);
		  break;
		}
	    }

	  free((void*) array_out);
	}

      mctop_wq_free(wq);
      mctop_alloc_free(alloc);

      mctop_free(topo);

      numa_free((void*) array, array_siz);
      numa_free_nodemask(nodemask);
    }
  return 0;
}

#include <atomics.h>

volatile uint32_t __barrier[2] = { 0, 0 };
volatile uint64_t __exited  = 0;

void
barrier_wait(const uint nb, const uint n_threads)
{
  FAI_U32(&__barrier[nb]);
  while (__barrier[nb] != n_threads)
    {
      PAUSE();
    }
}

uint
try_exit(const uint n_threads)
{
  do
    {
      uint cur_n_thr = __exited;
      if (cur_n_thr == (n_threads - 1))
	{
	  return 0;
	}
      if (CAS_U64(&__exited, cur_n_thr, cur_n_thr + 1) == cur_n_thr)
	{
	  return 1;
	}
    }
  while (1);
}

int
cmpfunc(const void* a, const void* b)
{
  return (*(int*)a - *(int*)b);
}

static inline void
bsort(int* arr, const uint len)
{
  uint n_swaps;
  do
    {
      n_swaps = 0;
      for (uint j = 1; j < len; j++)
	{
	  if (arr[j - 1] > arr[j])
	    {
	      int tmp = arr[j - 1];
	      arr[j - 1] = arr[j];
	      arr[j] = tmp;
	      n_swaps = 1;
	    }
	}
    }
  while (n_swaps);
}

static inline uint
wq_sort(wq_data_t* wpd)
{
  if (wpd->sorted == 1)
    {
      return 0;
    }

  wpd->sorted = 1;
  qsort(wpd->array, wpd->len, sizeof(int), cmpfunc);
  /* bsort(wpd->array, wpd->len); */
  return 1;
}

wq_data_t*
wq_merge(const wq_data_t* w0, const wq_data_t* w1)
{
  size_t len = w0->len + w1->len;
  int* anew = malloc_assert(len * sizeof(int));

  uint i0 = 0, i1 = 0, o = 0;
  while (i0 < w0->len && i1 < w1->len)
    {
      if (w0->array[i0] < w1->array[i1])
	{
	  anew[o++] = w0->array[i0++];
	}
      else
	{
	  anew[o++] = w1->array[i1++];
	}
    }

  while (i0 < w0->len)
    {
      anew[o++] = w0->array[i0++];
    }
  while (i1 < w1->len)
    {
      anew[o++] = w1->array[i1++];
    }

  if (w0->interleaved == 0)
    {
      free(w0->array);
    }
  if (w1->interleaved == 0)
    {
      free(w1->array);
    }

  free((void*) w0);
  free((void*) w1);

  return wq_data_create(0, 1, len, anew);
}

void*
test_pin(void* params)
{
  mctop_wq_t* wq = (mctop_wq_t*) params;
  mctop_alloc_t* alloc = wq->alloc;
  mctop_alloc_pin(alloc);

  mctop_wq_thread_enter(wq);

  while (1)
    {
      wq_data_t* wqd = mctop_wq_dequeue(wq);
      if (wqd != NULL)
	{
	  if (!wq_sort(wqd))
	    {
	      wq_data_t* wqd1 = mctop_wq_dequeue(wq);
	      if (wqd1 != NULL)
		{
		  wq_sort(wqd1);
		  wq_data_t* wqn = wq_merge(wqd, wqd1);
		  mctop_wq_enqueue(wq, wqn);
		}
	      else
		{
		  if (mctop_wq_is_last_thread(wq))
		    {
		      array_out = wqd->array;
		      free(wqd);
		      break;
		    }
		  mctop_wq_enqueue(wq, wqd);
		}
	    }
	  else
	    {
	      mctop_wq_enqueue(wq, wqd);
	    }
	}
      else
	{
	  break;
	}
    }

  //  barrier_wait(1, alloc->n_hwcs);
  mctop_wq_thread_exit(wq);
  mctop_wq_stats_print(wq);
  return NULL;
}
