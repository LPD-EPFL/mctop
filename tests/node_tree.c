#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>

void* test_pin(void* params);

int* memory;
int* memory_nodes[16];
size_t memory_size = 1024 * 1024 * 1024LL;
size_t memory_len;

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
      //      mctop_print(topo);
      mctop_alloc_t* alloc = mctop_alloc_create(topo, test_num_threads, test_num_hwcs_per_socket, test_policy);
      mctop_alloc_print(alloc);
      mctop_alloc_print_short(alloc);

      mctop_alloc_pin_nth_socket(alloc, 0);

      memory = malloc(memory_size);
      assert(memory);

      memory_len = memory_size / sizeof(*memory);
      for (uint i = 0; i < memory_len; i++)
	{
	  memory[i] = i;
	}


      mctop_node_tree_t* nt = mctop_alloc_node_tree_create(alloc, EVERYONE);
      mctop_node_tree_print(nt);

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
	      int rc = pthread_create(&threads[t], &attr, test_pin, nt);
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
      mctop_alloc_free(alloc);
      mctop_node_tree_free(nt);

      mctop_free(topo);
    }
  return 0;
}

void*
test_pin(void* params)
{
  mctop_node_tree_t* nt = (mctop_node_tree_t*) params;
  mctop_alloc_t* alloc = nt->alloc;

  mctop_alloc_pin(nt->alloc);
  /* mctop_alloc_thread_print(); */

  const uint my_node = mctop_alloc_thread_node_id();
  const uint my_node_id = mctop_alloc_thread_insocket_id();
  const size_t node_mem_size = memory_size / nt->n_nodes;
  const size_t my_mem_size = node_mem_size / mctop_alloc_get_num_hw_contexts_node(alloc, my_node);
  
  int* my_mem_in = memory + (my_node * (node_mem_size / sizeof(int)) + (my_node_id * (my_mem_size / sizeof(int))));
  const uint node_leader = (my_node_id == 0);
  if (node_leader)
    {
      memory_nodes[my_node] = malloc(node_mem_size);
      assert(memory_nodes[my_node]);
    }

  mctop_alloc_barrier_wait_node(alloc);
  MCTOP_F_STEP(s, a, b);
  int* my_mem_out = memory_nodes[my_node] + (my_node_id * (my_mem_size / sizeof(int)));
  memcpy(my_mem_out, my_mem_in, my_mem_size);
  mctop_alloc_barrier_wait_node(alloc);
  MCTOP_P_STEP("init", s, a, b, mctop_alloc_thread_is_node_leader());

  for (int l = mctop_node_tree_get_num_levels(nt) - 1; l >= 0; l--)
    {
      mctop_node_tree_work_t ntw;
      if (mctop_node_tree_get_work_description(nt, l, &ntw))
	{
	  mctop_node_tree_barrier_wait(nt, l);
	  if (node_leader)
	    {
	      printf("LVL%u -- Node %-2u - Role %u for %-2u <- %-2u -- offset id %u\n", l,
		     mctop_alloc_thread_node_id(), ntw.node_role, ntw.destination, ntw.source,
		     ntw.id_offset);
	    }
	}
      else
	{
	  break;
	}
    }

  mctop_node_tree_barrier_wait_all(nt);

  return NULL;
}
