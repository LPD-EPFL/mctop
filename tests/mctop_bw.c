#include <mctop_alloc.h>
#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#if __x86_64__
#  include <numaif.h>
#endif

typedef struct td
{
  uint id;
  uint n_hwcs;
  mctop_t* topo;
  uint size_mb;
  uint n_reps;
  double* bws;
  uint verbose;
  pthread_barrier_t* barrier;
} td_t;

void* test_bw(void* params);

int
main(int argc, char **argv) 
{
  size_t test_size_mb = 128;
  uint test_n_reps = 1;

  char mct_file[100];
  uint manual_file = 0;
  int test_num_threads = 0;
  uint test_verbose = 0;

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
      c = getopt_long(argc, argv, "hm:n:p:c:r:s:g:i:v", long_options, &i);

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
	case 's':
	  test_size_mb = atoi(optarg);
	  break;
	case 'r':
	  test_n_reps = atoi(optarg);
	  break;
	case 'v':
	  test_verbose = 1;
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
      uint n_hwcs = mctop_get_num_hwc_per_socket(topo);
      if (test_num_threads > 0)
	{
	  n_hwcs = test_num_threads;
	}
      pthread_t threads[n_hwcs];
      pthread_attr_t attr;
      void* status;

      td_t* tds = malloc(n_hwcs * sizeof(td_t));
      assert(tds);
      double* bws = malloc(n_hwcs * sizeof(double));
      assert(bws);

      pthread_barrier_t* barrier = malloc(sizeof(pthread_barrier_t));
      assert(barrier);
      pthread_barrier_init(barrier, NULL, n_hwcs);

      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

      for(size_t t = 0; t < n_hwcs; t++)
	{
	  tds[t].id = t;
	  tds[t].n_hwcs = n_hwcs;
	  tds[t].topo = topo;
	  tds[t].barrier = barrier;
	  tds[t].size_mb = test_size_mb;
	  tds[t].n_reps = test_n_reps;
	  tds[t].bws = bws;
	  tds[t].verbose = test_verbose;
	  if (pthread_create(&threads[t], &attr, test_bw, tds + t))
	    {
	      printf("mctop_sort ERROR: pthread_create()\n");
	      exit(-1);
	    }
	}

      pthread_attr_destroy(&attr);

      for(uint t = 0; t < n_hwcs; t++)
	{
	  if (pthread_join(threads[t], &status))
	    {
	      printf("mctop_sort ERROR: pthread_join()\n");
	      exit(-1);
	    }
	}

      free(tds);
      free(bws);
      mctop_free(topo);
    }
  return 0;
}


#define ID0(x) if(id == 0) { x };
#define MB(n) ((n) * (1024 * 1024LL))
#define MB_to_uin64(n) (MB(n) / sizeof(uint64_t))

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

enum
  {
    BW_READ,
    BW_WRITE,
  } rw;

double
mem_bw_estimate(volatile uint64_t* mem, const uint do_read, const size_t n, const size_t reps)
{
  volatile uint64_t* m0 = (uint64_t*) mem;
  size_t sum;
  volatile size_t suma = 0;

  const size_t n_warmup = n >> 2;
  for (size_t i = 0; i < n_warmup; i++)
    {
      sum = m0[i];
    }


  struct timespec start, stop;
  clock_gettime(CLOCK_REALTIME, &start);
  if (do_read == BW_READ)
    {
      for (uint r = 0; r < reps; r++)
	{
	  for (size_t i = 0; i < n; i++)
	    {
	      sum = m0[i];
	    }
	}
    }
  else if (do_read == BW_WRITE)
    {
      for (uint r = 0; r < reps; r++)
	{
	  for (size_t i = 0; i < n; i++)
	    {
	      m0[i] = 0xAAAAFFFF;
	    }
	}
    }
  clock_gettime(CLOCK_REALTIME, &stop);

  for (size_t i = 0; i < n_warmup; i++)
    {
      sum = m0[i];
    }


  struct timespec dur = timespec_diff(start, stop);
  double dur_s = dur.tv_sec + (dur.tv_nsec / 1e9);
  if (dur_s == 9)
    {
      printf("%p%zu%zu", m0, sum, suma);
    }

  double bw = (reps * n * sizeof(uint64_t)) / (1e9 * dur_s);
  return bw;
}


void*
test_bw(void* params)
{
  td_t* td = (td_t*) params;
  const uint id = td->id;
  mctop_t* topo = td->topo;
  const size_t size_mb = td->size_mb;
  for (uint s = 0; s < topo->n_sockets; s++)
    {
      mctop_run_on_socket(topo, s);
      pthread_barrier_wait(td->barrier);
      ID0(printf("# Socket #%u\n", s););

      for (uint n = 0; n < topo->n_sockets; n++)
	{
	  pthread_barrier_wait(td->barrier);

	  volatile size_t* mem = numa_alloc_onnode(MB(size_mb), n);
	  bzero((void*) mem, MB(size_mb));
	  ID0(printf("  # Node #%u :", n););

	  pthread_barrier_wait(td->barrier);

	  double bwr = mem_bw_estimate(mem, BW_READ, MB_to_uin64(size_mb), td->n_reps);
	  td->bws[id] = bwr;

	  pthread_barrier_wait(td->barrier);

	  numa_free((void*) mem, MB(size_mb));

	  double bwt = 0;
	  ID0(
	      for (uint i = 0; i < td->n_hwcs; i++) 
		{ 
		  bwt += td->bws[i]; 
		}
	      if (td->verbose)
		{
		  printf(" [");
		  uint i;
		  for (i = 0; i < td->n_hwcs - 1; i++) 
		    { 
		      printf("%.2f + ", td->bws[i]);
		    }
		  printf("%.2f] = ", td->bws[i]);
		}
	      printf("  %8.2f GB/s\n", bwt);
	      );
	}
    }

  return NULL;
}
