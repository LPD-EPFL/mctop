#include <mctop_crawler.h>
#include <mctop_mem.h>
#include <mctop_profiler.h>
#include <mctop.h>

const int test_num_threads = 2;
const int test_num_smt_threads = 2;
size_t test_num_reps = 10000;
const size_t test_num_smt_reps = 1e7;
const size_t test_num_dvfs_reps = 5e6;
const double test_smt_ratio = 0.75;
const double test_dvfs_ratio = 0.95;
uint test_dvfs = 0;
volatile int dvfs_up_hwc_ready = -1; /* which hw context is ready by the dvfs up thread */

size_t test_num_warmup_reps = 10000 >> 4;
size_t test_max_stdev = 7;
size_t test_max_stdev_max = 14;
size_t test_num_cache_lines = 1024;
size_t test_cdf_cluster_offset = 25;
int test_num_clusters_hint = 0;
cache_line_t* test_cache_line = NULL;

typedef enum
  {
    NONE,
    C_STRUCT,
    LAT_TABLE,
  } test_format_t;
typedef enum
  {
    AR_1D,
    AR_2D,
  } array_format_t;
test_format_t test_format = NONE;
int test_verbose = 0;
int test_num_hw_ctx;
ticks* lat_table = NULL;
ticks** mem_lat_table = NULL;
volatile int high_stdev_retry = 0;

int test_num_sockets = -1;	/* num nodes / sockets */

typedef enum 
  {
    NO_MEM,			/* no mem. lat measurements */
    ON_TIME,			/* mem. lat measurements in // with comm. latencies */
    ON_TOPO,			/* mem. lat measurements based on topology */
  } mctop_mem_type_t;
mctop_mem_type_t test_do_mem = ON_TOPO;
int test_mem_on_demand = 0;
const size_t test_mem_reps = 1e6;
const size_t test_mem_size = 128 * 1024 * 1024LL;
volatile uint64_t** node_mem;

const uint test_mem_bw_num_streams = 2;
const size_t test_mem_bw_size = 1024 * 1024 * 1024LL;
const uint test_mem_bw_num_reps = 4;
volatile uint32_t mem_bw_barrier = 0;
volatile double* mem_bw_gbps;
double** mem_bw_table;


void ll_random_create(volatile uint64_t* mem, const size_t size);
ticks ll_random_traverse(volatile uint64_t* list, const size_t reps);

static cache_line_t* cache_lines_create(const size_t size_bytes, const int on_node);
void cache_lines_destroy(cache_line_t* cl, const size_t size, const uint use_numa);
int lat_table_get_hwc_with_lat(ticks** lat_table, const size_t n, ticks target_lat, int* hwcs);
void print_lat_table(void* lat_table, const size_t n, const test_format_t test_format, array_format_t format);
ticks** lat_table_normalized_create(ticks* lat_table, const size_t n, cdf_cluster_t* cc);


UNUSED static uint64_t
fai_prof(cache_line_t* cl, const size_t reps, mctop_prof_t* profiler)
{
  volatile uint64_t c = 0;
  MCTOP_PROF_START(profiler, ticks_start);
  c = IAF_U64(cl->word);
  MCTOP_PROF_STOP(profiler, ticks_start, reps);
  return c;
}

UNUSED static uint64_t
cas_prof(cache_line_t* cl, const size_t reps, mctop_prof_t* profiler)
{
  volatile uint64_t c = 0;
  MCTOP_PROF_START(profiler, ticks_start);
  c = CAS_U64(cl->word, 0, 1);
  MCTOP_PROF_STOP(profiler, ticks_start, reps);
  return c;
}

#define ATOMIC_OP(a, b, c) cas_prof(a, b, c)

static void
hw_warmup(cache_line_t* cl, const size_t warmup_reps, barrier2_t* barrier2, const int tid, mctop_prof_t* profiler)
{
  for (size_t rep = 0; rep < (warmup_reps + 1); rep++)
    {
      if (tid == 0)
	{
	  barrier2_cross(barrier2, tid, rep);
	  ATOMIC_OP(cl, rep, profiler);
	}
      else
	{
	  ATOMIC_OP(cl, rep, profiler);
	  barrier2_cross(barrier2, tid, rep);      
	}
    }
  
}

#define ID0_DO(x) if (tid == 0) { x; }
#define ID1_DO(x) if (tid == 1) { x; }

static inline void
lat_table_2d_set(ticks* arr, const int col_size, const int row, const int col, ticks val)
{
  arr[(row * col_size) + col] = val;
}

static inline ticks
lat_table_2d_get(ticks* arr, const int col_size, const int row, const int col)
{
  return arr[(row * col_size) + col];
}

static inline size_t
abs_sub(const size_t a, const size_t b)
{
  if (a > b)
    {
      return a - b;
    }
  return b - a;
}

void*
crawl(void* param)
{
  thread_local_data_t* tld = (thread_local_data_t*) param;
  const int tid = tld->id;
  pthread_barrier_t* barrier_sleep = tld->barrier;
  barrier2_t* barrier2 = tld->barrier2;
  
  const double test_completion_perc_step = 100.0 / test_num_hw_ctx;
  double test_completion_perc = 0;
  double test_completion_time = 0;

  const uint _num_reps = test_num_reps; /* local copy */
  const uint _num_warmup_reps = test_num_warmup_reps; /* local copy */
  const uint _test_cl_size = test_num_cache_lines * sizeof(cache_line_t);
  const uint _num_sockets = test_num_sockets;
  mctop_mem_type_t _do_mem = test_do_mem;
  uint _mem_on_demand = test_mem_on_demand;
  const uint _num_hw_ctx = test_num_hw_ctx;
  const uint _verbose = test_verbose;
  const int _do_dvfs = test_dvfs;
  int max_stdev = test_max_stdev;

  mctop_prof_t* profiler = mctop_prof_create(_num_reps);
  mctop_prof_stats_t* stats = malloc_assert(sizeof(mctop_prof_stats_t));

  volatile size_t sum = 0;

  for (int x = 0; x < _num_hw_ctx; x++)
    {
      clock_t _clock_start = clock();
      if (tid == 0)
	{
	  set_cpu(x); 
	  if (_do_dvfs)
	    {
	      dvfs_scale_up(test_num_dvfs_reps, test_dvfs_ratio, NULL);
	    }

	  /* mem. latency measurements */
	  int node_local = -1;
	  if (unlikely(_do_mem == ON_TIME))
	    {
	      volatile ticks mem_lats[_num_sockets];
	      for (int n = 0; n < _num_sockets; n++)
		{
		  volatile uint64_t* l = node_mem[n];
		  volatile uint64_t* mem = NULL;
		  if (unlikely(_mem_on_demand))
		    {
		      mem = numa_alloc_onnode(test_mem_size, n);
		      ll_random_create(mem, test_mem_size);
		      l = mem;
		    }
		  mem_lats[n] = ll_random_traverse(l, test_mem_reps);
		  if (unlikely(_mem_on_demand))
		    {
		      numa_free((void*) mem, test_mem_size);
		    }
		}
	      ticks mem_lat_min = -1;
	      for (int n = 0; n < _num_sockets; n++)
		{
		  if (mem_lats[n] < mem_lat_min)
		    {
		      mem_lat_min = mem_lats[n];
		      node_local = n;
		    }
		  mem_lat_table[x][n] = mem_lats[n];
		}
	      numa_set_preferred(mem_lat_min);
	    }
	  cache_lines_destroy(test_cache_line, _test_cl_size, _do_mem == ON_TIME);
	  test_cache_line = cache_lines_create(_test_cl_size, node_local);
	}

      pthread_barrier_wait(barrier_sleep);
      volatile cache_line_t* cache_line = test_cache_line;

      size_t history_med[2] = { 0 };
      for (int y = x + 1; y < _num_hw_ctx; y++)
	{
	  ID1_DO(set_cpu(y););

	  hw_warmup(cache_line, _num_warmup_reps, barrier2, tid, profiler);

	  for (size_t rep = 0; rep < _num_reps; rep++)
	    {
	      barrier2_cross_explicit(barrier2, tid, 5);
	      if (likely(tid == 0))
		{
		  barrier2_cross(barrier2, tid, rep);
		  sum += ATOMIC_OP(cache_line, rep, profiler);
		}
	      else
		{
		  sum += ATOMIC_OP(cache_line, rep, profiler);
		  barrier2_cross(barrier2, tid, rep);      
		}
	    }

	  /* mctop_prof_stats */
	  mctop_prof_stats_calc(profiler, stats);
	  /* abs_deviation_t ad;							 */
	  /* get_abs_deviation(pfd_store[0], _num_reps, &ad); */
	  double stdev = stats->std_dev_perc;
	  size_t median = stats->median;
	  if (likely(tid == 0))
	    {
	      if (stdev > max_stdev && (history_med[0] != median || history_med[1] != median))
		{
		  high_stdev_retry = 1;
		}

	      if (unlikely(_verbose))
		{
		  printf(" [%02d->%02d] median %-4zu with stdv %-7.2f%% | limit %2d%% %s\n",
			 x, y, median, stdev, max_stdev,  high_stdev_retry ? "(high)" : "");
		}
	      lat_table_2d_set(lat_table, _num_hw_ctx, x, y, median);
	      history_med[0] = history_med[1];
	      history_med[1] = median;
	    }
	  else
	    {
	      lat_table_2d_set(lat_table, _num_hw_ctx, y, x, median);
	    }

	  barrier2_cross_explicit(barrier2, tid, 6);

	  if (unlikely(high_stdev_retry))
	    {
	      barrier2_cross_explicit(barrier2, tid, 7);
	      if (++max_stdev > test_max_stdev_max)
		{
		  max_stdev = test_max_stdev_max;
		}
	      high_stdev_retry = 0;
	      y--;
	    }
	  else
	    {
	      max_stdev = test_max_stdev;
	      history_med[0] = history_med[1] = 0;
	    }
	}

      if (tid == 0)
	{
	  clock_t _clock_stop = clock();
	  double sec = (_clock_stop - _clock_start) / (double) CLOCKS_PER_SEC;
	  test_completion_time += sec;
	  test_completion_perc += test_completion_perc_step;
	  printf(" %6.1f%% completed in %8.1f secs (step took %7.1f secs) \n",
		 test_completion_perc, test_completion_time, sec);
	  assert(sum != 0);
	}
    }

  if (tid == 0)
    {
      cache_lines_destroy(test_cache_line, _test_cl_size, _do_mem == ON_TIME);
      for (int x = 0; x < _num_hw_ctx; x++)
	{
	  for (int y = x + 1; y < _num_hw_ctx; y++)
	    {
	      ticks xy = lat_table_2d_get(lat_table, _num_hw_ctx, x, y);
	      lat_table_2d_set(lat_table, _num_hw_ctx, y, x, xy);
	    }
	}
    }

  mctop_prof_free(profiler);
  free(stats);
  return NULL;
}

void*
init_mem(void* param)
{
  const int tid = *(int*) param;
  numa_run_on_node(tid);
  node_mem[tid] = numa_alloc_onnode(test_mem_size, tid);
  assert(node_mem[tid] != NULL);
  volatile uint64_t* m = node_mem[tid];
  ll_random_create(m, test_mem_size);
  if (tid == 0 && test_num_sockets > 1)
    {
      ticks t1 = ll_random_traverse(node_mem[!tid], test_mem_reps);
      ticks t2 = ll_random_traverse(node_mem[!tid], test_mem_reps);
      ticks df = abs_sub(t1, t2);
      ticks avg = (t1 + t2) / 2;
      if (df > (0.2 * avg))
	{
	  printf("## Memory measurements inaccurate (try 1: %zu, 2: %zu) \n", t1, t2);

	  volatile uint64_t* m = numa_alloc_onnode(test_mem_size, !tid);
	  assert(m != NULL);
	  ll_random_create(m, test_mem_size);
	  ticks t1 = ll_random_traverse(node_mem[!tid], test_mem_reps);
	  numa_free((void*) m, test_mem_size);

	  m = numa_alloc_onnode(test_mem_size, !tid);
	  assert(m != NULL);
	  ll_random_create(m, test_mem_size);
	  ticks t2 = ll_random_traverse(node_mem[!tid], test_mem_reps);
	  numa_free((void*) m, test_mem_size);

	  ticks df = abs_sub(t1, t2);
	  ticks avg = (t1 + t2) / 2;
	  if (df < (0.2 * avg))
	    {
	      printf("## Will use on-demand memory allocation (try 1: %zu, 2: %zu) \n", t1, t2);
	      test_mem_on_demand = 1;
	    }
	}
    }
  return NULL;
}

void*
is_smt(void* param)
{
  thread_local_data_t* tld = (thread_local_data_t*) param;
  const int tid = tld->id;
  const int hwc = tld->hw_context;
  barrier2_t* barrier2 = tld->barrier2;
  pthread_barrier_t* barrier = tld->barrier;

  set_cpu(hwc);
  spin_time(test_num_smt_reps >> 8);
  barrier2_cross_explicit(barrier2, tid, 0);

  const int n_smt = 5;
  int is_smt_n = 0;
  for (int i = 0; i < 5; i++)
    {
      barrier2_cross_explicit(barrier2, tid, 2);
      ticks times[2];
      if (tid == 0)
	{
	  times[0] = spin_time(test_num_smt_reps);
	  pthread_barrier_wait(barrier);
	}
      else
	{
	  pthread_barrier_wait(barrier);
	}

      pthread_barrier_wait(barrier);
      barrier2_cross_explicit(barrier2, tid, 1);
      times[1] = spin_time(test_num_smt_reps);
      if (tid == 0)
	{
	  is_smt_n += (times[0] < (times[1] * test_smt_ratio));
	}
    }

  if (tid == 0)
    {
      int* smt = malloc_assert(sizeof(smt));
      *smt = (is_smt_n > (n_smt - is_smt_n));
      return smt;
    }
  return NULL;
}

static void
mini_barrier(volatile uint32_t* b, const uint n)
{
  IAF_U32(b);
  while (*b != n)
    {
      PAUSE();
    }
}

double
mem_bw_estimate(volatile cache_line_t** mem, const uint n_streams, const size_t n, const size_t reps)
{
  volatile cache_line_t* m0, *m1;
  m0 = mem[0];
  m1 = mem[1];
  volatile size_t sum[n_streams];

  struct timespec start, stop;
  clock_gettime(CLOCK_REALTIME, &start);
  for (uint r = 0; r < reps; r++)
    {
      for (size_t i = 0; i < n; i++)
	{
	  sum[0] = m0[i].word[0];
	  sum[1] = m1[i].word[0];
	}
    }
  clock_gettime(CLOCK_REALTIME, &stop);
  struct timespec dur = timespec_diff(start, stop);
  double dur_s = dur.tv_sec + (dur.tv_nsec / 1e9);

  if (dur_s == 9)
    {
      printf("%p%zu%p%zu", m0, sum[0], m1, sum[1]);
    }

  double bw = (n_streams * reps * n * sizeof(cache_line_t)) / (1e9 * dur_s);
  return bw;
}

void*
mem_bandwidth(void* param)
{
  thread_local_data_t* tld = (thread_local_data_t*) param;
  const int tid = tld->id;
  const uint n_threads = tld->n_threads;
  pthread_barrier_t* barrier = tld->barrier;
  mctopo_t* topo = tld->topo;
  const uint n_cls = test_mem_bw_size / sizeof(cache_line_t);

  volatile cache_line_t** mem_bw = malloc_assert(test_mem_bw_num_streams * sizeof(cache_line_t*));

  ID0_DO(mem_bw_gbps = (double*) malloc_assert(n_threads * sizeof(double)));

  for (int n = 0; n < mctop_get_num_nodes(topo); n++)
    {
      mctop_run_on_node(topo, n);
      ID0_DO(printf(" ######## Run Node %d\n", n);)
      for (int mem_on = 0; mem_on < mctop_get_num_nodes(topo); mem_on++)
	{
	  ID0_DO(printf(" #### Mem Node %d\n", mem_on));
	  for (int s = 0; s < test_mem_bw_num_streams; s++)
	    {
	      mem_bw[s] = numa_alloc_onnode(test_mem_bw_size, mem_on);
	      assert(mem_bw[s] != NULL);
	      bzero((void*) mem_bw[s], test_mem_bw_size);
	    }

	  pthread_barrier_wait(barrier);
	  dvfs_scale_up(test_num_dvfs_reps, test_dvfs_ratio, NULL);
	  mini_barrier(&mem_bw_barrier, n_threads);

	  double bw_gbps = mem_bw_estimate(mem_bw, test_mem_bw_num_streams, n_cls, test_mem_bw_num_reps);
	  mem_bw_gbps[tid] = bw_gbps;

	  pthread_barrier_wait(barrier);

	  for (int s = 0; s < test_mem_bw_num_streams; s++)
	    {
	      numa_free((void*) mem_bw[s], test_mem_bw_size);
	    }

	  if (tid == 0)
	    {
	      mem_bw_barrier = 0;
	      double tot_bw = 0;
	      printf("   BW (");
	      for (int i = 0; i < n_threads; i++)
		{
		  printf("+%2.2f", mem_bw_gbps[i]);
		  tot_bw += mem_bw_gbps[i];
		}
	      printf(") = %f\n", tot_bw);
	      mem_bw_table[n][mem_on] = tot_bw;
	    }
	}
    }

  return NULL;
}

int
main(int argc, char **argv) 
{
  test_num_hw_ctx = get_num_hw_ctx();
  double dvfs_up_dur;
  test_dvfs = dvfs_scale_up(test_num_dvfs_reps, test_dvfs_ratio, &dvfs_up_dur);

  struct option long_options[] = 
    {
      // These options don't set a flag
      {"help",                      no_argument,       NULL, 'h'},
      {"num-cores",                 required_argument, NULL, 'n'},
      {"mem",                       required_argument, NULL, 'm'},
      {"num-sockets",               required_argument, NULL, 's'},
      {"cdf-offset",                required_argument, NULL, 'c'},
      {"num-clusters",              required_argument, NULL, 'i'},
      {"repetitions",               required_argument, NULL, 'r'},
      {"format",                    required_argument, NULL, 'f'},
      {"verbose",                   no_argument,       NULL, 'v'},
      {NULL, 0, NULL, 0}
    };

  int i;
  char c;
  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hvn:c:r:f:s:m:i:", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) 
	{
	case 0:
	  /* Flag is automatically set */
	  break;
	case 'h':
	  printf("mctop  Copyright (C) 2016  Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>\n"
		 "This program comes with ABSOLUTELY NO WARRANTY.\n"
		 ".\n"
		 "Usage: ./mctop [options...]\n"
		 "\n"
		 "Options:\n"
		 ">>> BASIC SETTINGS\n"
		 "  -r, --repetitions <int>\n"
		 "        Number of repetitions per iteration (default=" XSTR(DEFAULT_NUM_REPS) ")\n"
		 "  -m, --mem <int>\n"
		 "        Do (NUMA) memory latency measurements (default=" XSTR(DEFAULT_MEM) ")\n"
		 "        0: no mem. latency measurements\n"
		 "        1: do mem. latency measurements while measuring communication latencies (slow)\n"
		 "        2: do mem. latency measurements per-node after constructing the topology\n"
		 "  -c, --cdf-offset <int>\n"
		 "        How many cycles should the min and the max elements of two adjacent core\n"
		 "        clusters differ to consider them distinct? (default=" XSTR(DEFAULT_NUM_REPS) ")\n"
		 "  -f, --format <int>\n"
		 "        Output format (default=" XSTR(DEFAULT_FORMAT) "). Supported formats:\n"
		 "        0: none, 1: c/c++ struct, 2: latency table\n"
		 ">>> SECONDARY SETTINGS"
		 "  -n, --num-cores <int>\n"
		 "        Up to how many hardware contexts to run on (default=all cores)\n"
		 "  -s, --num-sockets <int>\n"
		 "        How many sockets (i.e., NUMA nodes) to assume if -n is given (default=all sockets)\n"
		 "  -i, --num-clusters <int>\n"
		 "        Hint on how many latency groups to look for (default=disabled). For example, on a 2-socket\n"
		 "        Intel server with HyperThreads, we expect 4 groups (i.e., hyperthread, core, socket, cross socket)."
		 ">>> AUXILLIARY SETTINGS\n"
		 "  -h, --help\n"
		 "        Print this message\n"
		 "  -v, --verbose\n"
		 "        Verbose printing of results (default=" XSTR(DEFAULT_VERBOSE) ")\n"
		 );
	  exit(0);
	case 'n':
	  test_num_hw_ctx = atoi(optarg);
	  break;
	case 'm':
	  test_do_mem = atoi(optarg);
	  break;
	case 's':
	  test_num_sockets = atoi(optarg);
	  break;
	case 'f':
	  test_format = atoi(optarg);
	  break;
	case 'r':
	  test_num_reps = atoi(optarg);
	  break;
	case 'c':
	  test_cdf_cluster_offset = atoi(optarg);
	  break;
	case 'i':
	  test_num_clusters_hint = atoi(optarg);
	  break;
	case 'v':
	  test_verbose = 1;
	  break;
	case '?':
	  printf("Use -h or --help for help\n");
	  exit(0);
	default:
	  exit(1);
	}
    }


  if (test_num_sockets < 0)
    {
      test_num_sockets = numa_num_task_nodes();
    }

  printf("# MCTOP Settings:\n");
  printf("#   Repetitions    : %zu\n", test_num_reps);
  printf("#   Do-memory      : %d\n", test_do_mem);
  printf("#   Cluster-offset : %zu\n", test_cdf_cluster_offset);
  printf("#   # Cores        : %d\n", test_num_hw_ctx);
  printf("#   # Sockets      : %d\n", test_num_sockets);
  printf("#   # Hint         : %d clusters\n", test_num_clusters_hint);
  printf("#   CPU DVFS       : %d (Freq. up in %zu ms)\n", test_dvfs, !test_dvfs ? 0 : (size_t) dvfs_up_dur);
  printf("# Progress\n");

  pthread_t threads_mem[test_num_sockets];
  pthread_t threads[test_num_threads];
  pthread_attr_t attr;
  pthread_attr_init(&attr);  /* Initialize and set thread detached attribute */
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  if (test_do_mem == ON_TIME)
    {
      node_mem = calloc_assert(test_num_sockets, sizeof(uint64_t*));

      clock_t start = clock();
      int ids[test_num_sockets];
      for(int t = 0; t < test_num_sockets; t++)
	{
	  ids[t] = t;
	  int rc = pthread_create(&threads_mem[t], &attr, init_mem, ids + t);
	  if (rc)
	    {
	      printf("ERROR; return code from pthread_create() is %d\n", rc);
	      exit(-1);
	    }
	}
    
    
      for(int t = 0; t < test_num_sockets; t++) 
	{
	  void* status;
	  int rc = pthread_join(threads_mem[t], &status);
	  if (rc) 
	    {
	      printf("ERROR; return code from pthread_join() is %d\n", rc);
	      exit(-1);
	    }
	}
      clock_t stop = clock();
      printf("# Mem init took: %.1f secs\n", (stop - start) / (double) CLOCKS_PER_SEC);
    }


  thread_local_data_t* tds = (thread_local_data_t*) malloc_assert(test_num_threads * sizeof(thread_local_data_t));
  barrier2_t* barrier2 = barrier2_create();
  pthread_barrier_t* barrier = malloc_assert(sizeof(pthread_barrier_t));
  pthread_barrier_init(barrier, NULL, test_num_smt_threads);

  lat_table = calloc_assert(test_num_hw_ctx * test_num_hw_ctx, sizeof(ticks));
  if (test_do_mem != NO_MEM)
    {
      mem_lat_table = (ticks**) table_calloc(test_num_hw_ctx, test_num_sockets, sizeof(ticks));
    }

#define MCTOP_PREDEFINED_LAT_TABLE 0
#if MCTOP_PREDEFINED_LAT_TABLE == 0
  for(int t = 0; t < test_num_threads; t++)
    {
      tds[t].id = t;
      tds[t].n_threads = test_num_threads;
      tds[t].barrier = barrier;
      tds[t].barrier2 = barrier2;
      int rc = pthread_create(&threads[t], &attr, crawl, tds + t);
      if (rc)
	{
	  printf("ERROR; return code from pthread_create() is %d\n", rc);
	  exit(-1);
	}
    }
    
    
  for(int t = 0; t < test_num_threads; t++) 
    {
      void* status;
      int rc = pthread_join(threads[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
    }



  print_lat_table(lat_table, test_num_hw_ctx, test_format, AR_1D);

  const size_t lat_table_size = test_num_hw_ctx * test_num_hw_ctx;
  cdf_t* cdf = cdf_calc(lat_table, lat_table_size);
  /* cdf_print(cdf); */
  cdf_cluster_t* cc = cdf_cluster(cdf, test_cdf_cluster_offset, test_num_clusters_hint);
  if (cc == NULL)
    {
      fprintf(stderr, "*** Error: Could not create an appropriate clusterings of cores. Rerun mctop with:\n"
	      "\t1. more repetitions (-r)\n"
	      "\t2. on time mem. latency measurements (-m1)\n");
      exit(-1);
    }

  cdf_cluster_print(cc);
  ticks** lat_table_norm = lat_table_normalized_create(lat_table, test_num_hw_ctx, cc);
  print_lat_table(lat_table_norm, test_num_hw_ctx, test_format, AR_2D);
  ticks min_lat = cdf_cluster_get_min_latency(cc);
  int* possible_smt_hwcs = calloc_assert(test_num_smt_threads, sizeof(int));
  if (!lat_table_get_hwc_with_lat(lat_table_norm, test_num_hw_ctx, min_lat, possible_smt_hwcs))
    {
      fprintf(stderr, "** Cannot find 2 hw contects to check for STM! Single core machine?\n");
    }

  /* SMT detection */
  for(int t = 0; t < test_num_smt_threads; t++)
    {
      tds[t].id = t;
      tds[t].n_threads = test_num_smt_threads;
      tds[t].barrier = barrier;
      tds[t].barrier2 = barrier2;
      tds[t].hw_context = possible_smt_hwcs[t];
      int rc = pthread_create(&threads[t], &attr, is_smt, tds + t);
      if (rc)
	{
	  printf("ERROR; return code from pthread_create() is %d\n", rc);
	  exit(-1);
	}
    }
    
  int is_smt_cpu = 0;
  for(int t = 0; t < test_num_smt_threads; t++) 
    {
      void* status;
      int rc = pthread_join(threads[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
      if (t == 0)
	{
	  is_smt_cpu = *(int *) status;
	  free(status);
	}
    }

  printf("## CPU is SMT: %d\n", is_smt_cpu);
  mctopo_t* topo = mctopo_construct(lat_table_norm, test_num_hw_ctx, mem_lat_table, test_num_sockets, cc, is_smt_cpu);

#else
  int is_smt_cpu = is_smt1;
  test_num_sockets = n_sockets1;
  test_num_hw_ctx = n_hwcs1;
  const int n = test_num_hw_ctx;
  ticks** lat_table_norm = (ticks**) table_malloc(n, n, sizeof(ticks));

  for (int x = 0; x < test_num_hw_ctx; x++)
    {
      for (int y = 0; y < test_num_hw_ctx; y++)
	{
	  lat_table_norm[x][y] = lat_table1[x][y];
	}
    }
  mctopo_t* topo = mctopo_construct(lat_table_norm, test_num_hw_ctx, mem_lat_table, test_num_sockets, NULL, is_smt_cpu);
#endif

  if (test_do_mem == ON_TOPO)
    {
      printf("## Calculating memory latencies on topology\n");
      mctopo_mem_latencies_calc(topo, mem_lat_table);
    }

  mem_bw_table = (double**) table_malloc(test_num_sockets, test_num_sockets, sizeof(double));
  uint test_num_mem_bw_threads = mctop_get_num_cores_per_socket(topo);
  printf("## Calculating memory bw on topology using %u cores\n", test_num_mem_bw_threads);
  pthread_t threads_mem_bw[test_num_mem_bw_threads];
  pthread_barrier_t* barrier_mem_bw = malloc_assert(sizeof(pthread_barrier_t));
  pthread_barrier_init(barrier_mem_bw, NULL, test_num_mem_bw_threads);

  thread_local_data_t* tds_mem_bw = (thread_local_data_t*) malloc_assert(test_num_mem_bw_threads * sizeof(thread_local_data_t));
  for(int t = 0; t < test_num_mem_bw_threads; t++)
    {
      tds_mem_bw[t].id = t;
      tds_mem_bw[t].n_threads = test_num_mem_bw_threads;
      tds_mem_bw[t].barrier = barrier_mem_bw;
      tds_mem_bw[t].topo = topo;
      int rc = pthread_create(&threads_mem_bw[t], &attr, mem_bandwidth, tds_mem_bw + t);
      if (rc)
	{
	  printf("ERROR; return code from pthread_create() is %d\n", rc);
	  exit(-1);
	}
    }
    
  for(int t = 0; t < test_num_mem_bw_threads; t++) 
    {
      void* status;
      int rc = pthread_join(threads_mem_bw[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
    }
  free(tds_mem_bw);
  free(barrier_mem_bw);

  mctopo_mem_bandwidth_add(topo, mem_bw_table);
  table_free((void**) mem_bw_table, test_num_sockets);

  /* Free attribute and wait for the other threads */
  pthread_attr_destroy(&attr);

  mctopo_print(topo);


#if MCTOP_PREDEFINED_LAT_TABLE == 0
  if (test_do_mem == ON_TIME)
    {
      for (int n = 0; n < test_num_sockets; n++)
	{
	  numa_free((void*) node_mem[n], test_mem_size);
	}
      free(node_mem);

      for (int n = 0; n < test_num_hw_ctx; n++)
	{
	  free(mem_lat_table[n]);
	}
      free(mem_lat_table);
    }

  table_free((void**) lat_table_norm, test_num_hw_ctx);
  cdf_cluster_free(cc);
  cdf_free(cdf);
  free(tds);
  free(barrier2);
  free(lat_table);
#endif
}


/* ******************************************************************************** */
/* help functions */
/* ******************************************************************************** */

cache_line_t*
cache_lines_create(const size_t size_bytes, const int on_node)
{
  cache_line_t* cls = mctop_mem_alloc_local(size_bytes, on_node);
  return cls;
}

inline void
cache_lines_destroy(cache_line_t* cl, const size_t size, const uint numa_lib)
{
  if (likely(cl != NULL))
    {
      mctop_mem_free((void*) cl, size, numa_lib);
    }
}

void 
print_lat_table(void* lt, const size_t n, const test_format_t test_format, array_format_t format)
{
  ticks* lat_table_1d = (ticks*) lt;
  ticks** lat_table_2d = (ticks**) lt;
  if (test_format != NONE)
    {
      printf("## Output ################################################################\n");
    }
  switch (test_format)
    {
    case C_STRUCT:
      printf("size_t lat_table[%zu][%zu] = \n{\n", n, n);
      for (int x = 0; x < n; x++)
	{
	  printf("  { ");
	  for (int y = 0; y < n; y++)
	    {
	      ticks lat = (format == AR_2D) ? lat_table_2d[x][y] : lat_table_2d_get(lat_table_1d, n, x, y);
	      printf("%-3zu, ", lat);
	    }
	  printf("},\n");
	}
      printf("};\n");
      break;
    case LAT_TABLE:
      printf("     ");
      for (int y = 0; y < n; y++)
	{
	  printf("%-3d ", y);
	}
      printf("\n");

      /* print lat table */
      for (int x = 0; x < n; x++)
	{
	  printf("[%02d] ", x);
	  for (int y = 0; y < n; y++)
	    {
	      ticks lat = (format == AR_2D) ? lat_table_2d[x][y] : lat_table_2d_get(lat_table_1d, n, x, y);
	      printf("%-3zu ", lat);
	    }
	  printf("\n");
	}
      break;
    case NONE:
      break;
    }
  if (test_format != NONE)
    {
      printf("##########################################################################\n");
    }
}

int
lat_table_get_hwc_with_lat(ticks** lat_table, const size_t n, ticks target_lat, int* hwcs)
{
  for (int x = 0; x < n; x++)
    {
      for (int y = 0; y < n; y++)
	{
	  if (lat_table[x][y] == target_lat)
	    {
	      hwcs[0] = x;
	      hwcs[1] = y;
	      return 1;
	    }
	}
    }
  return 0;
}


ticks**
lat_table_normalized_create(ticks* lat_table, const size_t n, cdf_cluster_t* cc)
{
  ticks** lat_table_norm = malloc_assert(n * sizeof(ticks*));
  for (int i = 0; i < n ; i++)
    {
      lat_table_norm[i] = malloc_assert(n * sizeof(ticks));
    }

  for (size_t x = 0; x < n; x++)
    {
      for (size_t y = 0; y < n; y++)
	{
	  ticks lat = lat_table_2d_get(lat_table, n, x, y);
	  lat_table_norm[x][y] = cdf_cluster_value_to_cluster_median(cc, lat);
	}
    }

  return lat_table_norm;
}
