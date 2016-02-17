#ifndef __H_MCTOP__
#define __H_MCTOP__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>

#include <helper.h>
#include <barrier.h>
#include <pfd.h>
#include <cdf.h>

typedef volatile struct cache_line
{
  volatile uint64_t word[CACHE_LINE_SIZE / sizeof(uint64_t)];
} cache_line_t;

typedef struct thread_local_data
{
  int id;
  barrier2_t* barrier2;
} thread_local_data_t;

#endif	/* __H_MCTOP__ */
