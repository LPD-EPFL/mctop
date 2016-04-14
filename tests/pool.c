#include <mctop_alloc.h>
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
  mctop_alloc_policy test_policy = MCTOP_ALLOC_SEQUENTIAL;
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

      mctop_alloc_pool_t* ap = mctop_alloc_pool_create(topo);
      
      mctop_alloc_t* alloc = mctop_alloc_pool_get_alloc(ap, test_num_threads,
							test_num_hwcs_per_socket, test_policy);
      for (uint i = 0; i < 100; i++)
	{
	  const mctop_alloc_policy p = rand() % MCTOP_ALLOC_NUM;
	  const uint n_hwcs = rand() % topo->n_hwcs;
	  const uint n_config = MCTOP_ALLOC_ALL;
	  alloc = mctop_alloc_pool_get_alloc(ap, n_hwcs, n_config, p);
	}

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
	      int rc = pthread_create(&threads[t], &attr, test_pin, alloc);
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

      mctop_alloc_pool_free(ap);

      mctop_free(topo);
    }
  return 0;
}

void*
test_pin(void* params)
{
  mctop_alloc_t* alloc = (mctop_alloc_t*) params;

  mctop_alloc_barrier_wait_all(alloc);

  /* const size_t reps = 1e9; */

  mctop_alloc_pin_plus(alloc);

  for (int r = 0; r < 1; r++)
    {
      mctop_alloc_thread_print();

      //      for (volatile size_t i = 0; i < reps; i++) { __asm volatile ("nop"); }

      mctop_alloc_barrier_wait_node(alloc);

      /* mctop_alloc_unpin(); */
      /* mctop_alloc_thread_print(); */
      //      for (volatile size_t i = 0; i < reps; i++) { __asm volatile ("nop"); }
    }

  return NULL;
}
