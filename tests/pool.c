#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>

void* test_pin(void* params);

pthread_barrier_t* barrier;

volatile uint reps;
const size_t spin_for = 3e9;
int
main(int argc, char **argv) 
{
  char mct_file[100];
  uint manual_file = 0;
  int test_num_threads = 2;
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
      c = getopt_long(argc, argv, "hm:n:r:", long_options, &i);

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
	case 'r':
	  test_run_pin = atoi(optarg);
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

  reps = test_run_pin;

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

      barrier = malloc(sizeof(pthread_barrier_t));
      assert(barrier != NULL);
      pthread_barrier_init(barrier, NULL, test_num_threads);

      mctop_alloc_pool_t* ap = mctop_alloc_pool_create(topo);
      
      srand(time(NULL));

      if (test_run_pin)
	{
	  const uint n_hwcs = test_num_threads;
	  pthread_t threads[n_hwcs];
	  pthread_attr_t attr;
	  void* status;
    
	  /* Initialize and set thread detached attribute */
	  pthread_attr_init(&attr);
	  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
	  for(int t = 0; t < n_hwcs - 1; t++)
	    {
	      int rc = pthread_create(&threads[t], &attr, test_pin, ap);
	      if (rc)
		{
		  printf("ERROR; return code from pthread_create() is %d\n", rc);
		  exit(-1);
		}
	    }

	  pthread_attr_destroy(&attr);


	  for (uint i = 0; i < reps; i++)
	    {
	      const mctop_alloc_policy p = rand() % MCTOP_ALLOC_NUM;
	      const uint n_config = MCTOP_ALLOC_ALL;
	      mctop_alloc_pool_set_alloc(ap, n_hwcs, n_config, p);
	      pthread_barrier_wait(barrier);
	      mctop_alloc_pool_pin(ap);
	      mctop_alloc_thread_print(); fflush(stdout);
	      for (volatile size_t i = 0; i < spin_for; i++);
	      pthread_barrier_wait(barrier);
	    }

	  pthread_barrier_wait(barrier);

	  for(int t = 0; t < n_hwcs - 1; t++)
	    {
	      int rc = pthread_join(threads[t], &status);
	      if (rc) 
		{
		  printf("ERROR; return code from pthread_join() is %d\n", rc);
		  exit(-1);
		}
	    }
	}

      free(barrier);
      mctop_alloc_pool_free(ap);
      mctop_free(topo);
    }
  return 0;
}

void*
test_pin(void* params)
{
  mctop_alloc_pool_t* ap = (mctop_alloc_pool_t*) params;

  for (uint i = 0; i < reps; i++)
    {
      pthread_barrier_wait(barrier);
      mctop_alloc_pool_pin(ap);
      mctop_alloc_thread_print(); fflush(stdout);
      for (volatile size_t i = 0; i < spin_for; i++);
      pthread_barrier_wait(barrier);
    }

  pthread_barrier_wait(barrier);


  return NULL;
}
