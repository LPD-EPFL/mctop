#include <mctop.h>

const int test_num_threads = 2;
size_t test_num_reps = 10000;
size_t test_num_warmup_reps = 10000 >> 4;
size_t test_max_stdev = 7;
size_t test_max_stdev_max = 14;
size_t test_num_cache_lines = 1024;
cache_line_t* test_cache_line = NULL;
int test_verbose = 0;
int test_num_hw_ctx;
ticks* lat_table;
volatile int high_stdev_retry = 0;


static cache_line_t* cache_lines_create(const size_t num);
inline void cache_lines_destroy(cache_line_t* cl);


UNUSED static uint64_t
fai_prof(cache_line_t* cl, const volatile size_t reps)
{
  volatile uint64_t c = 0;
  PFDI(0);
  c = IAF_U64(cl->word);
  PFDO(0, reps);
  return c;
}

UNUSED static uint64_t
cas_prof(cache_line_t* cl, const volatile size_t reps)
{
  volatile uint64_t c = 0;
  PFDI(0);
  c = CAS_U64(cl->word, 0, 1);
  PFDO(0, reps);
  return c;
}

#define ATOMIC_OP(a, b) fai_prof(a, b)

static void
dvfs_warmup(cache_line_t* cl, const size_t warmup_reps, barrier2_t* barrier2, const int tid)
{
  for (size_t rep = 0; rep < (warmup_reps + 1); rep++)
    {
      if (tid == 0)
	{
	  ATOMIC_OP(cl, rep);
	  barrier2_cross(barrier2, tid, rep);
	}
      else
	{
	  barrier2_cross(barrier2, tid, rep);      
	  ATOMIC_OP(cl, rep);
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

void*
crawl(void* param)
{
  thread_local_data_t* tld = (thread_local_data_t*) param;
  const int tid = tld->id;
  barrier2_t* barrier2 = tld->barrier2;
  
  const double test_completion_perc_step = 100.0 / test_num_hw_ctx;
  double test_completion_perc = 100.0 / test_num_hw_ctx;
  double test_completion_time = 0;

  int max_stdev = test_max_stdev;

  PFDINIT(test_num_reps);

  set_cpu(tid);

  volatile size_t sum = 0;

  for (int x = 0; x < test_num_hw_ctx; x++)
    {
      clock_t _clock_start = clock();
      if (tid == 0)
	{
	  set_cpu(x); 
	  PFDTERM_INIT(test_num_reps);
	  cache_lines_destroy(test_cache_line);
	  test_cache_line = cache_lines_create(test_num_cache_lines);
	}
      barrier2_cross_explicit(barrier2, tid, 8);
      volatile cache_line_t* cache_line = test_cache_line;

      size_t history_med[2] = { 0 };
      for (int y = x + 1; y < test_num_hw_ctx; y++)
	{
	  ID1_DO(set_cpu(y); PFDTERM_INIT(test_num_reps));

	  dvfs_warmup(cache_line, test_num_warmup_reps, barrier2, tid);

	  for (size_t rep = 0; rep < test_num_reps; rep++)
	    {
	      barrier2_cross_explicit(barrier2, tid, 5);
	      if (tid == 0)
		{
		  sum += ATOMIC_OP(cache_line, rep);
		  barrier2_cross(barrier2, tid, rep);
		}
	      else
		{
		  barrier2_cross(barrier2, tid, rep);      
		  sum += ATOMIC_OP(cache_line, rep);
		}
	    }

	  abs_deviation_t ad;							
	  get_abs_deviation(pfd_store[0], test_num_reps, &ad);
	  double stdev = 100 * (1 - (ad.avg - ad.std_dev) / ad.avg);
	  size_t median = ad.median;
	  if (tid == 0)
	    {
	      /* if (test_verbose) */
	      /* 	{ */
	      /* 	  /\* PFDPN(0, test_num_reps, 0); *\/ */
	      /* 	} */
	      if (stdev > max_stdev && (history_med[0] != median || history_med[1] != median))
		{
		  high_stdev_retry = 1;
		}

	      if (test_verbose)
		{
		  printf(" [%02d->%02d] median %-4zu with stdv %-5.2f%% | limit %2d%% %s\n",
			 x, y, median, stdev, max_stdev,  high_stdev_retry ? "(high)" : "");
		}
	      lat_table_2d_set(lat_table, test_num_hw_ctx, x, y, median);
	      history_med[0] = history_med[1];
	      history_med[1] = median;
	    }
	  else
	    {
	      lat_table_2d_set(lat_table, test_num_hw_ctx, y, x, median);
	    }

	  barrier2_cross_explicit(barrier2, tid, 6);
	  if (high_stdev_retry)
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
	  printf(" %6.1f%% completed in %-8.1f secs (step took %-7.1f secs) \n",
		 test_completion_perc, test_completion_time, sec);
	  /* printf("sum = %zu\n", sum); */
	  assert(sum != 0);
	}
    }

  ID0_DO(cache_lines_destroy(test_cache_line));
  PFDTERM();
  return NULL;
}


  int
    main(int argc, char **argv) 
  {
    test_num_hw_ctx = get_num_hw_ctx();

    struct option long_options[] = 
      {
	// These options don't set a flag
	{"help",                      no_argument,       NULL, 'h'},
	{"num-cores",                 required_argument, NULL, 'n'},
	{"verbose",                   no_argument,       NULL, 'v'},
	{NULL, 0, NULL, 0}
      };

    int i;
    char c;
    while(1) 
      {
	i = 0;
	c = getopt_long(argc, argv, "hvn:r:", long_options, &i);

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
		   ">>> SECONDARY SETTINGS"
		   "  -n, --num-cores <int>\n"
		   "        Up to how many hardware contexts to run on (default=all cores)\n"
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
	  case 'r':
	    test_num_reps = atoi(optarg);
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


    pthread_t threads[test_num_threads];
    pthread_attr_t attr;
    pthread_attr_init(&attr);  /* Initialize and set thread detached attribute */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    thread_local_data_t* tds = (thread_local_data_t*) malloc(test_num_threads * sizeof(thread_local_data_t));
    assert(tds != NULL);
    barrier2_t* barrier2 = barrier2_create();
    lat_table = calloc(test_num_hw_ctx * test_num_hw_ctx, sizeof(ticks));
    assert(lat_table != NULL);

    for(int t = 0; t < test_num_threads; t++)
      {
	tds[t].id = t;
	tds[t].barrier2 = barrier2;
	int rc = pthread_create(&threads[t], &attr, crawl, tds + t);
	if (rc)
	  {
	    printf("ERROR; return code from pthread_create() is %d\n", rc);
	    exit(-1);
	  }
      }
    
    /* Free attribute and wait for the other threads */
    pthread_attr_destroy(&attr);
    
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

    printf("     ");
    for (int y = 0; y < test_num_hw_ctx; y++)
      {
	printf("%-3d ", y);
      }
    printf("\n");

    /* print lat table */
    for (int x = 0; x < test_num_hw_ctx; x++)
      {
	printf("[%02d] ", x);
	for (int y = 0; y < test_num_hw_ctx; y++)
	  {
	    printf("%-3zu ", lat_table_2d_get(lat_table, test_num_hw_ctx, x, y));
	  }
	printf("\n");
      }


    const size_t lat_table_size = test_num_hw_ctx * test_num_hw_ctx;
    cdf_t* cdf = cdf_calc(lat_table, lat_table_size);
    /* cdf_print(cdf); */
    cdf_cluster(cdf, 25);

    cdf_free(cdf);
    free(tds);
    free(barrier2);
    free(lat_table);
  }

  /*  */
  static cache_line_t*
    cache_lines_create(const size_t num)
  {
    cache_line_t* cls = malloc(num * sizeof(cache_line_t));
    assert(cls != NULL);
    for (volatile size_t i = 0; i < num; i++)
      {
	cls[i].word[0] = cls[i].word[2] = cls[i].word[3] = cls[i].word[4] = 0;
      }
    return cls;
  }

  inline void
    cache_lines_destroy(cache_line_t* cl)
  {
    if (likely(cl != NULL))
      {
	free((void*) cl);
      }
  }


