#ifndef __H_MERGE_UTILS__
#define __H_MERGE_UTILS__

#include "merge_sse_utils.h"
#define SORT_TYPE uint

#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)

static inline void merge_arrays_unaligned_sse(SORT_TYPE* a, SORT_TYPE* b, SORT_TYPE* dest, size_t num_a, size_t num_b) {
    //first take care of portions flowing over the 16-byte boundaries
    size_t i,j,k;

    i=0;
    j=0;
    k=0;

    // Try to align the two arrays. This should happen fast: Francis, Mathieson. "A Benchmark Parallel Sort for Shared Memory Multiprocessors"
    while((((((uintptr_t)&(a[i])) & 15) != 0) || ((((uintptr_t)&(b[j])) & 15) != 0)) && ((i<num_a) && (j<num_b)))
    {
        if (a[i] < b[j])
            dest[k++] = a[i++];

        else
            dest[k++] = b[j++];
    }

    if (i == num_a || j == num_b) {
      while (i < num_a)
          dest[k++] = a[i++];

      while (j < num_b)
          dest[k++] = b[j++];
      return;
    }

    size_t sizea = num_a - i;
    size_t sizeb = num_b - j;
    size_t size_a_cutoff = sizea & 0xFFFFFFFFFFFFFFFCL;
    size_t size_b_cutoff = sizeb & 0xFFFFFFFFFFFFFFFCL;

    sse_merge_aligned_32bit((void *)&a[i], (void *)&b[j], (void *)&dest[k], size_a_cutoff, size_b_cutoff);

    i += size_a_cutoff;
    j += size_b_cutoff;
    k = i+j;

    // bubble down the remainder of the elements
    for (size_t counter = i; counter < num_a; counter++) {
      dest[k] = a[counter];
      uint temp;
      size_t element = k;
      while (dest[element] < dest[element-1]) {
        temp = dest[element-1];
        dest[element-1] = dest[element];
        dest[element] = temp;
        element--;
      }
      k++;
    }

    for (size_t counter = j; counter < num_b; counter++) {
      dest[k] = b[counter];
      uint temp;
      size_t element = k;
      while (dest[element] < dest[element-1]) {
        temp = dest[element-1];
        dest[element-1] = dest[element];
        dest[element] = temp;
        element--;
      }
      k++;
    }
}




static inline void merge_arrays(SORT_TYPE *a, SORT_TYPE *b, SORT_TYPE *dest, size_t sizea, size_t sizeb, long myid, long num_threads, long *help_array_a, long *help_array_b, pthread_barrier_t *barrier_start, pthread_barrier_t *barrier_end) {
  long size1, size2, desti;

  // Evenly distribute the arrays: Francis, Mathieson. "A Benchmark Parallel Sort for Shared Memory Multiprocessors"
  long alpha = 0, alpha_prime = 0, beta = 0, beta_prime = 0;
  if (myid > 0) {
  long gamma = ((myid) * (sizea + sizeb) / num_threads);
  long alpha_min = max(0, (gamma-(sizeb+1)));
  long alpha_max = min((long)sizea+1, gamma);
  long alpha_prime_min = alpha_min;
  long alpha_prime_max = alpha_max;
  long length = gamma;
  while ((alpha_prime_min + 1) != alpha_prime_max) {
    alpha_prime = (alpha_prime_min + alpha_prime_max) / 2;
    beta_prime = length - alpha_prime;
    if (a[alpha_prime] <= b[beta_prime])
      alpha_prime_min = alpha_prime;
    else
      alpha_prime_max = alpha_prime;
  }
  if (a[alpha_prime_min] <= b[length-alpha_prime_max])
    alpha = alpha_prime_max;
  else
    alpha = alpha_prime_min;
    beta = length - alpha;
  }

  help_array_a[myid] = alpha;
  help_array_b[myid] = beta;

  pthread_barrier_wait(barrier_start);
  if (myid < num_threads - 1) {
    size1 = help_array_a[myid+1] - help_array_a[myid];
    size2 = help_array_b[myid+1] - help_array_b[myid];
  }
  else {
    size1 = sizea - help_array_a[myid];
    size2 = sizeb - help_array_b[myid];
  }
  desti = alpha+beta;

  //printf("[thread %ld / %d] res1 %ld res2 %ld desti = %ld size1 = %ld size2 = %ld &a[res1] = %ld &b[res2] = %ld\n", pthread_self(), myid, a, b, desti, size1, size2, (uintptr_t) &a[a], (uintptr_t) &b[b]);
  merge_arrays_unaligned_sse(&a[alpha], &b[beta], &dest[desti], size1, size2);
  pthread_barrier_wait(barrier_end);
}

#endif
