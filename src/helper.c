#include <helper.h>

int
set_cpu(int cpu) 
{
  int ret = 1;
#if defined(__sparc__)
  if (processor_bind(P_LWPID, P_MYID, cpu, NULL) == -1)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }
#elif defined(__tile__)
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, cpu)) < 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");
    }

  if (cpu != tmc_cpus_get_my_cpu())
    {
      PRINT("******* i am not CPU %d", tmc_cpus_get_my_cpu());
    }

#else
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }
#endif

  return ret;
}

int
get_num_hw_ctx()
{
  int nc = 0;
  while (1)
    {
      if (!set_cpu(nc))
  	{
  	  break;
  	}
      nc++;
    }
  printf("** found #cores: %d\n", nc);
  return nc;
}
