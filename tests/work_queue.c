#include <mctop.h>
#include <pthread.h>
#include <getopt.h>

void* test_pin(void* params);

int
main(int argc, char **argv) 
{
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
      c = getopt_long(argc, argv, "hm:n:p:c:r", long_options, &i);

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
      mctop_wq_print(wq);

      if (test_run_pin)
	{
	  const uint n_hwcs = mctop_alloc_get_num_hw_contexts(alloc);
	  pthread_t threads[n_hwcs];
	  pthread_attr_t attr;
	  void* status;
    
	  /* Initialize and set thread detached attribute */
	  pthread_attr_init(&attr);
	  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
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
	}

      mctop_wq_free(wq);
      mctop_alloc_free(alloc);

      mctop_free(topo);
    }
  return 0;
}



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

struct wq_data
{
  uint from;
  uint node;
  size_t len;
  size_t array[0];
};


static inline struct wq_data*
data_alloc(const size_t len)
{
  struct wq_data* data = malloc(sizeof(struct wq_data) +
				(len + 1) * sizeof(size_t));      
  data->from = mctop_alloc_get_hw_context_id();
  data->node = mctop_alloc_get_local_node();
  data->len = len;
  return data;
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

  
static inline size_t
array_sum(const size_t* a, const uint n)
{
  size_t sum = 0;
  for (size_t i = 0; i < n; i++)
    {
      sum += a[i];
    }
  return sum;
}

const uint _multi   = 1024;
const uint _min     = 1;
const uint _max     = 16;
const uint _creps   = 2048;

void*
test_pin(void* params)
{
  mctop_wq_t* wq = (mctop_wq_t*) params;
  mctop_alloc_t* alloc = wq->alloc;
  mctop_alloc_pin(alloc);

  unsigned long* seeds = seed_rand();
  const uint hwcid = mctop_alloc_get_hw_context_id();
  const uint node = mctop_alloc_get_local_node();

  //  const uint _creps_n = _creps * (node + 1);
  const uint _creps_n = _creps * ((2 * node) + 1);
  /* const uint _creps_n = _creps * 2; */

  size_t len_tot = 0, n_chunks = 0;;

  for (size_t r = 0; r < _creps_n; r++)
    {
      n_chunks++;
      const size_t len = (_min + (mrand(seeds) % _max)) * _multi;
      len_tot += len;
      struct wq_data* data = data_alloc(len);
      for (size_t i = 0; i < len; i++)
	{
	  data->array[i] = i;
	}
      mctop_wq_enqueue(wq, data);
    }

  uint n_local = 0, n_total = 0;
  uint* n_from = calloc_assert(8, sizeof(uint));

  printf("[%2d@%d] Initialized %-5zu chunks / %-10zu elems\n", hwcid, node, n_chunks, len_tot);
  barrier_wait(0, alloc->n_hwcs);

  size_t sum = 0;
  while (1)
    {
      struct wq_data* data = mctop_wq_dequeue(wq);
      if (data != NULL && data->len > 1)
	{
	  n_total++;
	  n_local += (node == data->node);
	  n_from[data->node]++;

	  sum += array_sum(data->array, data->len);
	  free(data);
	}
      else
	{
	  struct wq_data* data_new = data_alloc(1);
	  data_new->array[0] = sum;
	  if (data != NULL)
	    {
	      data_new->array[0] += data->array[0];
	      free(data);
	    }
	  mctop_wq_enqueue(wq, data_new);

	  if (try_exit(alloc->n_hwcs))
	    {
	      break;
	    }
	  else
	    {
	      printf(" Accum results!"); fflush(stdout);
	      size_t sum = 0;
	      do
		{
		  struct wq_data* data = mctop_wq_dequeue(wq);
		  if (data == NULL)
		    {
		      break;
		    }
		  assert(data->len == 1);
		  sum += data->array[0];
		}
	      while (1);
	      printf(" = %zu\n", sum);
	      break;
	    }
	}
    }

  printf("[%2d@%d] #Tot %-5u / #Loc / %-5u #Rem %-5u [ %-5u %-5u %-5u %-5u %-5u %-5u %-5u %-5u ]\n",
	 hwcid, node, n_total, n_local, n_total - n_local,
	 n_from[0], n_from[1], n_from[2], n_from[3],
	 n_from[4], n_from[5], n_from[6], n_from[7]);

  barrier_wait(1, alloc->n_hwcs);

  mctop_wq_stats_print(wq);

  free(n_from);
  free(seeds);
  return NULL;
}
