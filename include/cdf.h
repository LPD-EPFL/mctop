/*   
 *   File: helper.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: commulative distribution functions
 *
 *
 */

#ifndef __H_CDF__
#define __H_CDF__

#include <helper.h>
#include "mctop.h"

#define XSTR(s)                         STR(s)
#define STR(s)                          #s
#define UNUSED         __attribute__ ((unused))

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

cdf_t* cdf_calc(uint64_t* vals, size_t n_vals);
void cdf_free(cdf_t* cdf);
void cdf_print(cdf_t* cdf);

cdf_cluster_t* cdf_cluster(cdf_t* cdf, const uint sensitivity, const uint target_n_clusters);
cdf_cluster_t* cdf_cluster_create_empty(const int n_clusters);

void cdf_cluster_free(cdf_cluster_t* cc);
void cdf_cluster_print(cdf_cluster_t* cc);
uint64_t cdf_cluster_get_min_latency(cdf_cluster_t* cc);
uint64_t cdf_cluster_value_to_cluster_median(cdf_cluster_t* cc, const uint64_t val);



#endif	/* __H_CDF__ */
