#include <mctop_crawler.h>
#include <mctop.h>

int
main(int argc, char **argv) 
{
  size_t test_mem_size = 128 * 1024 * 1024LL;

  struct option long_options[] = 
    {
      // These options don't set a flag
      {"help",                      no_argument,       NULL, 'h'},
      {"mem",                       required_argument, NULL, 'm'},
      {"verbose",                   no_argument,       NULL, 'v'},
      {NULL, 0, NULL, 0}
    };

  int i;
  char c;
  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hvn:c:r:f:s:m", long_options, &i);

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
	  exit(0);
	case 'm':
	  test_mem_size = atoi(optarg);
	  break;
	case '?':
	  printf("Use -h or --help for help\n");
	  exit(0);
	default:
	  exit(1);
	}
    }


  for (int i = 0; i < 4; i++)
    {
      mctop_set_cpu(i);
      volatile uint64_t* mem = malloc(test_mem_size);
      ll_random_create(mem, test_mem_size);
      volatile uint64_t* l = mem;
      const uint mem_lat_reps = 1e6;
      volatile ticks __s = getticks();
      for (int r = 0; r < mem_lat_reps; r++)
	{
	  l = (uint64_t*) *l;
	}
      volatile ticks __e = getticks();
      ticks lat = (__e - __s) / mem_lat_reps;
      printf("set_cpu@%02d : %zu\n", i, lat);

      free((void*) mem);
    }

  
  for (int i = 0; i < numa_num_task_nodes(); i++)
    {
      numa_run_on_node(i);
      volatile uint64_t* mem = malloc(test_mem_size);
      ll_random_create(mem, test_mem_size);
      volatile uint64_t* l = mem;
      const uint mem_lat_reps = 1e6;
      volatile ticks __s = getticks();
      for (int r = 0; r < mem_lat_reps; r++)
	{
	  l = (uint64_t*) *l;
	}
      volatile ticks __e = getticks();
      ticks lat = (__e - __s) / mem_lat_reps;
      printf("numa_run_on_node@%02d : %zu\n", i, lat);

      free((void*) mem);
    }


  for (int x = 0; x < numa_num_task_nodes(); x++)
    {
      for (int i = 0; i < numa_num_task_nodes(); i++)
	{
	  mctop_set_cpu(x);
	  volatile uint64_t* mem = numa_alloc_onnode(test_mem_size, i);
	  ll_random_create(mem, test_mem_size);
	  volatile uint64_t* l = mem;
	  const uint mem_lat_reps = 1e6;
	  volatile ticks __s = getticks();
	  for (int r = 0; r < mem_lat_reps; r++)
	    {
	      l = (uint64_t*) *l;
	    }
	  volatile ticks __e = getticks();
	  ticks lat = (__e - __s) / mem_lat_reps;
	  printf("set_cpu(%d),allocon@%02d : %zu\n", x, i, lat);

	  /* free((void*) mem); */
	  numa_free((void*) mem, test_mem_size);
	}
    }


  for (int x = 0; x < numa_num_task_nodes(); x++)
    {
      volatile uint64_t* mem = numa_alloc_onnode(test_mem_size, x);
      ll_random_create(mem, test_mem_size);

      for (int i = 0; i < numa_num_task_nodes(); i++)
	{
	  mctop_set_cpu(i);
	  volatile uint64_t* l = mem;
	  const uint mem_lat_reps = 1e6;
	  volatile ticks __s = getticks();
	  for (int r = 0; r < mem_lat_reps; r++)
	    {
	      l = (uint64_t*) *l;
	    }
	  volatile ticks __e = getticks();
	  ticks lat = (__e - __s) / mem_lat_reps;
	  printf("allocon(%d), set_cpu(%d) : %zu\n", x, i, lat);

	  /* free((void*) mem); */
	}
      numa_free((void*) mem, test_mem_size);
    }
}


