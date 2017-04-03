#ifndef __H_MCTOP_CRAWLER__
#define __H_MCTOP_CRAWLER__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>

#if __x86_64__
#include <numa.h>
#endif

#include <helper.h>
#include <barrier.h>
#include <cdf.h>
#include <darray.h>
#include <mctop.h>

/* ******************************************************************************** */
/* default config */
/* ******************************************************************************** */

const int test_num_threads         = 2;
const int test_num_smt_threads     = 2;
const size_t test_num_smt_reps     = 1e7;
const size_t test_num_dvfs_reps    = 5e6;
const double test_smt_ratio        = 0.9;
const double test_dvfs_ratio       = 0.95;

const size_t test_mem_reps         = 1e6;
const size_t test_mem_size         = 512 * 1024 * 1024LL;
const uint test_mem_bw_num_streams = 2;
const uint test_mem_bw_num_reps    = 4;

#define DEFAULT_NUM_REPS           2000
#define DEFAULT_MAX_STDEV          7
#define DEFAULT_NUM_CACHE_LINES    1024
#define DEFAULT_CLUSTER_OFFS       20
#define DEFAULT_HINT               0
#define DEFAULT_MEM_AUGMENT        0
#define DEFAULT_FORMAT             MCT_FILE
#define DEFAULT_VERBOSE            0
#define DEFAULT_DO_MEM             ON_TOPO_BW
#define DEFAULT_MEM_BW_SIZE        2048 /* in MB */
#define DEFAULT_MEM_BW_MULTI       (1024 * 1024LL)

typedef enum
  {
    NONE,
    C_STRUCT,
    LAT_TABLE,
    MCT_FILE,
  } test_format_t;

const char* test_format_desc[] =
  {
    "None",
    "C struct",
    "Table",
    "MCT description file",
  };

typedef enum
  {
    AR_1D,
    AR_2D,
  } array_format_t;

typedef enum 
  {
    NO_MEM,			/* no mem. lat measurements */
    ON_TIME,			/* mem. lat measurements in // with comm. latencies */
    ON_TOPO,			/* mem. lat measurements based on topology */
    ON_TOPO_BW,			/* mem. lat + bw measurements based on topology */
  } mctop_test_mem_type_t;

const char* mctop_test_mem_type_desc[4] =
  {
    "No",
    "Latency only while communicating",
    "Latency only on topology",
    "Latency+Bandwidth on topology",
  };

typedef volatile struct cache_line
{
  volatile uint64_t word[CACHE_LINE_SIZE / sizeof(uint64_t)];
} cache_line_t;


#endif	/* __H_MCTOP_CRAWLER__ */
