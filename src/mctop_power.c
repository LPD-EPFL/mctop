#include <mctop.h>
#include <mctop_internal.h>


#if MCTOP_POWER == 1
#  include "rapl_read.h"

size_t
do_access_mem(volatile uint64_t* mem, const size_t n, volatile int* go)
{
  volatile uint64_t sum = 0;
  while (*go == 1)
    {
      for (size_t i = 0; i < n; i++)
	{
	  sum = mem[i];
	}
    }
  return sum;
}

void*
do_power(void* param)
{
  const size_t pow_mem_size = 1 * 1024 * 1024 * 1024LL;
  const size_t pow_n_elems = pow_mem_size / sizeof(uint64_t);

  tld_t* tld = (tld_t*) param;
  const int tid = tld->id;
  pthread_barrier_t* barrier = tld->barrier;
  mctop_t* topo = tld->topo;
  /* const uint n_nodes = mctop_get_num_nodes(topo); */
  mctop_set_cpu(topo, tid);
  /* const uint node = mctop_hwcid_get_local_node(topo, tid); */
  const uint nth_hwc_socket = mctop_hwcid_get_nth_hwc_in_socket(topo, tid);
  const uint nth_hwc_core = mctop_hwcid_get_nth_hwc_in_core(topo, tid);
  const uint nth_core_socket = mctop_hwcid_get_nth_core_in_socket(topo, tid);

  volatile uint64_t* mem = malloc_assert(pow_mem_size);
  bzero((void*) mem, pow_mem_size);
  pthread_barrier_wait(barrier);
 
  /* for (int n = -1; n < (int) n_nodes; n++) */
  /*   { */
  pthread_barrier_wait(barrier);
  /* if (n < 0 || n == node) */
  /* 	{ */
  if (nth_hwc_socket == 0) /* only first core, first hwcs */
    {
      if (do_access_mem(mem, pow_n_elems, tld->run) == 1234231)	{ printf("X"); };
    }
  pthread_barrier_wait(barrier);
  pthread_barrier_wait(barrier);
  if (nth_core_socket <= 1 && nth_hwc_core == 0) /* only first 2 hwcs, 2 cores */
    {
      if (do_access_mem(mem, pow_n_elems, tld->run) == 1234231)	{ printf("X"); };
    }
  pthread_barrier_wait(barrier);
  pthread_barrier_wait(barrier);
  if (nth_core_socket == 0) /* 2hwcs 1 core */
    {
      if (do_access_mem(mem, pow_n_elems, tld->run) == 1234231)	{ printf("X"); };
    }
  pthread_barrier_wait(barrier); 
  pthread_barrier_wait(barrier); 
  if (nth_hwc_core == 0) /* all cores */
    {
      if (do_access_mem(mem, pow_n_elems, tld->run) == 1234231)	{ printf("X"); };
    }
  pthread_barrier_wait(barrier);
  pthread_barrier_wait(barrier);
  /* everyone */
  if (do_access_mem(mem, pow_n_elems, tld->run) == 1234231) { printf("X"); };
  /* } */
  /* else */
  /* 	{ */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	  pthread_barrier_wait(barrier); */
  /* 	} */
  pthread_barrier_wait(barrier);
  /* } */

  free((void*) mem);
  return NULL;
}

const int ALL = -1;

inline void
MCTOP_POW_PRINT(char* type, rapl_stats_t* s, const uint n_sockets, const int sp)
{
  if (sp == ALL)				
    {									
      printf("## Power %-12s [all]: %7.2f %7.2f %7.2f %7.2f %7.2f \n",
	     type, 
	     s->power_pp0[n_sockets], 
	     s->power_rest[n_sockets], 
	     s->power_package[n_sockets], 
	     s->power_dram[n_sockets], 
	     s->power_total[n_sockets]
	     );
      for (uint i = 0; i < n_sockets; i++)
	{
	  printf("##       %-12s [%3d]: %7.2f %7.2f %7.2f %7.2f %7.2f \n",
		 type, i, s->power_pp0[i], s->power_rest[i], 
		 s->power_package[i], s->power_dram[i], s->power_total[i]
		 );
	}
    }
  else
    {
      printf("## Power %-12s [%3d]: %7.2f  %7.2f  %7.2f  %7.2f  %7.2f \n",
	     type, sp, s->power_pp0[sp],
	     s->power_rest[sp], s->power_package[sp], s->power_dram[sp], s->power_total[sp]
	     );
    }
 }

static void
mctop_pow_measure_one(char* msg, 
		      rapl_stats_t* s, 
		      pthread_barrier_t* barrier_pow, 
		      volatile int* run,
		      const uint n_sockets,
		      const int socket)
{
  *run = 1;
  pthread_barrier_wait(barrier_pow);
  RR_START_UNPROTECTED_ALL();
  sleep(1);
  RR_STOP_UNPROTECTED_ALL();
  *run = 0;
  RR_STATS(s);
  MCTOP_POW_PRINT(msg, s, n_sockets, socket);
  pthread_barrier_wait(barrier_pow);
}


static void
mctop_pow_copy_vals_diff(double* to[MCTOP_POW_TYPE_NUM], rapl_stats_t* s, rapl_stats_t* sub, const uint n_sockets)
{
  if (sub == NULL)
    {
      for (uint u = 0; u <= n_sockets; u++)
	{
	  to[u][CORES] = s->power_pp0[u];
	  to[u][REST] = s->power_rest[u];
	  to[u][PACKAGE] = s->power_package[u];
	  to[u][DRAM] = s->power_dram[u];
	  to[u][TOTAL] = s->power_total[u];
	}
    }
  else
    {
      for (uint u = 0; u <= n_sockets; u++)
	{
	  to[u][CORES] = s->power_pp0[u] - sub->power_pp0[u];
	  to[u][REST] = s->power_rest[u] - sub->power_rest[u];
	  to[u][PACKAGE] = s->power_package[u] - sub->power_package[u];
	  to[u][DRAM] = s->power_dram[u] - sub->power_dram[u];
	  to[u][TOTAL] = s->power_total[u] - sub->power_total[u];
	}
    }
}

void
mctop_power_measurements_free(mctop_t* topo, double*** m)
{
  for (uint i = 0; i < MCTOP_POW_TYPE_NUM; i++)
    {
      for (uint s = 0; s <= topo->n_sockets; s++)
	{
	  free(m[i][s]);
	}
      free(m[i]);
    }
  free(m);
}

double***
mctop_power_measurements(mctop_t* topo)
{
  RR_INIT_ALL();

  pthread_attr_t attr;
  pthread_attr_init(&attr);  /* Initialize and set thread detached attribute */
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  uint n_hwcs = topo->n_hwcs;
  pthread_t threads_pow[n_hwcs];
  pthread_barrier_t* barrier_pow = malloc_assert(sizeof(pthread_barrier_t));
  pthread_barrier_init(barrier_pow, NULL, n_hwcs + 1);  

  volatile int* run = malloc_assert(CACHE_LINE_SIZE);

  tld_t* tds = (tld_t*) malloc_assert(n_hwcs * sizeof(tld_t));
  for (int t = 0; t < n_hwcs; t++)
    {
      tds[t].id = t;
      tds[t].n_threads = n_hwcs;
      tds[t].barrier = barrier_pow;
      tds[t].topo = topo;
      tds[t].run = run;
      int rc = pthread_create(&threads_pow[t], &attr, do_power, tds + t);
      if (rc)
	{
	  printf("ERROR; return code from pthread_create() is %d\n", rc);
	  exit(-1);
	}
    }
    
  double*** pow_measurements = mctop_power_measurements_create(topo->n_sockets);

  pthread_barrier_wait(barrier_pow);

  RR_START_UNPROTECTED_ALL();
  sleep(1);
  RR_STOP_UNPROTECTED_ALL();
  rapl_stats_t s_idle, s;
  RR_STATS(&s_idle);
  MCTOP_POW_PRINT("idle", &s_idle, topo->n_sockets, ALL);

  uint n_measur = 0;
  mctop_pow_copy_vals_diff(pow_measurements[n_measur++], &s_idle, NULL, topo->n_sockets);

  
  /* 
     0 : idle
     1 : extra cost for 1st core
     2 : extra cost for 2nd core
     3 : extra cost for 2nd hwc of 1 core
     4 : all cores
     5 : all hwcs
  */

  /* for (int n = -1; n < (int) mctop_get_num_nodes(topo); n++) */
  /*   { */
  rapl_stats_t s_core1;
  mctop_pow_measure_one("1st core", &s_core1, barrier_pow, run, topo->n_sockets, ALL);
  mctop_pow_copy_vals_diff(pow_measurements[n_measur++], &s_core1, &s_idle, topo->n_sockets);

  mctop_pow_measure_one("1st 2 cores", &s, barrier_pow, run, topo->n_sockets, ALL);
  mctop_pow_copy_vals_diff(pow_measurements[n_measur++], &s, &s_core1, topo->n_sockets);

  mctop_pow_measure_one("2hwcs 1 core", &s, barrier_pow, run, topo->n_sockets, ALL);
  mctop_pow_copy_vals_diff(pow_measurements[n_measur++], &s, &s_core1, topo->n_sockets);

  mctop_pow_measure_one("all cores", &s, barrier_pow, run, topo->n_sockets, ALL);
  mctop_pow_copy_vals_diff(pow_measurements[n_measur++], &s, NULL, topo->n_sockets);

  mctop_pow_measure_one("all hwcs", &s, barrier_pow, run, topo->n_sockets, ALL);
  mctop_pow_copy_vals_diff(pow_measurements[n_measur++], &s, NULL, topo->n_sockets);
  /* } */

  for (int t = 0; t < n_hwcs; t++) 
    {
      void* status;
      int rc = pthread_join(threads_pow[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
    }
  free((void*) run);
  free(tds);
  free(barrier_pow);

  RR_TERM();
  return pow_measurements;
}

#endif	/*  MCTOP_POWER == 1 */
