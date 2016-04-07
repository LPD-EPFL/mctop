#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <parallel/algorithm>
#include <numa.h>
#include <mctop_alloc.h>
#include "merge_utils.h"

#define RAND_RANGE(N) ((double)rand() / ((double)RAND_MAX + 1) * (N))
#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)

#define mrand(x) xorshf96(&x[0], &x[1], &x[2])

static inline unsigned long*
seed_rand()
{
  unsigned long* seeds;
  seeds = (unsigned long*) malloc(64);
  seeds[0] = 11233311;
  seeds[1] = 2123123;
  seeds[2] = 313131222;
  return seeds;
}

//Marsaglia's xorshf generator
static inline unsigned long
xorshf96(unsigned long* x, unsigned long* y, unsigned long* z)  //period 2^96-1
{
  unsigned long t;
  (*x) ^= (*x) << 16;
  (*x) ^= (*x) >> 5;
  (*x) ^= (*x) << 1;

  t = *x;
  (*x) = *y;
  (*y) = *z;
  (*z) = t ^ (*x) ^ (*y);

  return *z;
}

unsigned long* seeds;

//pthread_mutex_t global_lock;

void readArray2(SORT_TYPE [], const long);
void printArray2(SORT_TYPE [], const long, const int);
void is_sorted(SORT_TYPE [], const long);

SORT_TYPE *a __attribute__((aligned(64)));
SORT_TYPE *res __attribute__((aligned(64)));

long *help_array_a __attribute__((aligned(64)));
long *help_array_b __attribute__((aligned(64)));

long in_socket_partitions;
long threads_per_partition;


//partition merge arguments
typedef struct merge_args_t
{
    SORT_TYPE* src;
    long sizea;
    SORT_TYPE* dest;
    SORT_TYPE** dest_ptr;
    long partitions;
    long *partition_starts;
    long *partition_sizes;
    pthread_barrier_t *round_barrier;
    long threads_per_partition;
    long nthreads;
    mctop_alloc_t *alloc;
} merge_args_t;


//binary search for a value in a vector
long bs(SORT_TYPE* data, SORT_TYPE size, SORT_TYPE value) {
    long start = 0;
    long end = size-1;
    while (start <= end)
    {
        long mid = (start+end)/2;
        if (data[mid] < value) {
            start = mid + 1;
        }
        else if (data[mid] > value) {
            end = mid - 1;
        }
        else
            return mid;
    }
    if (start > end) return start;
    return end;
}

#define MCTOP_P_STEP(__steps, __a, __b)		\
  {						\
    __b = mctop_getticks();			\
    mctop_ticks __d = __b - __a;		\
    printf("Step %zu : %-10zu cycles = %-10zu us\n",	\
	   __steps++, __d, __d / 2100);			\
    __a = __b;						\
  }




void merge(SORT_TYPE *src, SORT_TYPE *dest, long *partition_starts, long *partition_sizes, long partitions, long threads_per_partition, long nthreads) {

  long my_id = mctop_alloc_thread_core_insocket_id();
  
  long next_merge, partition_size, pos_in_merge, partition1, partition_a_start, partition_a_size, partition_b_start, partition_b_size;
  
  next_merge = my_id / threads_per_partition;
  long next_partition = next_merge * 2;
  while (1) {
    if (next_partition >= partitions || ((partitions % 2) && (next_partition >= partitions-1)))
      break;

    pos_in_merge = my_id % threads_per_partition;
    
    partition_a_start = partition_starts[next_partition];
    partition_a_size = partition_sizes[next_partition];
    partition_b_start = partition_starts[next_partition+1];
    partition_b_size = partition_sizes[next_partition+1];

    printf("[thread %d] getting to work on merge %ld, partition_1 %ld partition_2 %ld, size_a %ld, size_b %ld, dest %ld pos_in_merge %ld\n", mctop_alloc_thread_core_insocket_id(), next_merge, partition_a_start, partition_b_start, partition_a_size, partition_b_size, partition_a_start, pos_in_merge); fflush(stdout);
    SORT_TYPE *my_a = &src[partition_a_start];
    SORT_TYPE *my_b = &src[partition_b_start];
    SORT_TYPE *my_dest = &dest[partition_a_start];

    merge_arrays(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
    next_partition += (nthreads / threads_per_partition) * 2;
  }
  return;
}

void *merge(void *args) {
  merge_args_t *myargs = (merge_args_t *)args;
  mctop_alloc_pin(myargs->alloc);
  SORT_TYPE *src = myargs->src;
  SORT_TYPE *dest = myargs->dest;
  SORT_TYPE *temp;
  long size = myargs->sizea;
  long partitions = myargs->partitions;
  long threads_per_partition = myargs->threads_per_partition;
  long nthreads = myargs->nthreads;
  while (partitions > 1) {
    pthread_barrier_wait(myargs->round_barrier);
    if (mctop_alloc_thread_core_insocket_id() == 0) {
      printf("doing round %ld\n", partitions); 
      printf(" partition_start  partition_size\n");
      for(long i=0; i < partitions; i++){
        printf(" %5ld  %5ld\n", myargs->partition_starts[i], myargs->partition_sizes[i]);
      }
    }
    merge(src, dest, myargs->partition_starts, myargs->partition_sizes, partitions, threads_per_partition, nthreads);
    pthread_barrier_wait(myargs->round_barrier);
    if (mctop_alloc_thread_core_insocket_id() == 0) {
      for (long i = 0; i < partitions/2; i++) {
        myargs->partition_starts[i] = myargs->partition_starts[i*2];
        myargs->partition_sizes[i] = myargs->partition_sizes[i*2] + myargs->partition_sizes[i*2+1];
      }
      printf("ending round\n"); 
      if (partitions % 2) {
        myargs->partition_starts[partitions / 2] = myargs->partition_starts[partitions-1];
        myargs->partition_sizes[partitions / 2] = myargs->partition_sizes[partitions-1];
        memcpy((void*) &dest[myargs->partition_starts[partitions-1]], (void *) &src[myargs->partition_starts[partitions-1]], myargs->partition_sizes[partitions-1]*sizeof(SORT_TYPE));
      }
      //printArray2(dest, size, 0);
      printf("ending round\n"); 
    }
    if (partitions % 2)
      partitions++;
    partitions = partitions / 2;
    pthread_barrier_wait(myargs->round_barrier);
    temp = src;
    src = dest;
    dest = temp;
  }
  if (mctop_alloc_thread_core_insocket_id() == 0) {
    //printf("[threads 0]: src %lu, dest %lu, myargs->src %lu myargs->dest %lu\n", (uintptr_t)src, (uintptr_t)dest, (uintptr_t) myargs->src, (uintptr_t)myargs->dest);
    if (*myargs->dest_ptr != src)
       *(myargs->dest_ptr) = src;
    
  }
  return NULL;
}


int main(int argc,char *argv[]){
    long n, i, threads;
    struct timeval start, stop;
    unsigned long usec;
    mctop_alloc_policy allocation_policy;
    assert(argc >= 6);
    
    seeds = seed_rand();
    long array_size_mb = atol(argv[1]);

    n = array_size_mb * 1024 * (1024 / sizeof(SORT_TYPE));
    //n = n & 0xFFFFFFFFFFFFFFF0L;
    //n = array_size_mb;
    
    assert (n%4==0);
    a = (SORT_TYPE*) numa_alloc_onnode(n*sizeof(SORT_TYPE), 0);
    res = (SORT_TYPE*) numa_alloc_onnode(n*sizeof(SORT_TYPE), 0);

    threads = atoi(argv[2]);
    assert(threads>0);

    if (argc > 4)
      srand(atoi(argv[4]));
    else
      srand(42);

    if (argc > 5)
      in_socket_partitions = atoi(argv[5]);
    else
      in_socket_partitions = threads;
 
    if (argc > 6)
      threads_per_partition = atoi(argv[6]);
    else
      threads_per_partition = threads;

    allocation_policy = (mctop_alloc_policy) atoi(argv[3]);
    mctop_t * topo = mctop_load(NULL);
    mctop_alloc_t *alloc = mctop_alloc_create(topo, threads, MCTOP_ALLOC_ALL, allocation_policy);
    mctop_alloc_print_short(alloc);

    readArray2(a, n);
    long *partition_starts = (long *)malloc(in_socket_partitions * sizeof(long));
    long *partition_sizes = (long *)malloc(in_socket_partitions * sizeof(long));
    // sort in partitions
    for (long i = 0; i < in_socket_partitions-1; i++){
      __gnu_parallel::sort(a + (i * (n / in_socket_partitions)), a + ((i+1) * (n / in_socket_partitions)));
      partition_starts[i] = (i * (n / in_socket_partitions));
      partition_sizes[i] = (n / in_socket_partitions);
      printf("partition %ld: %ld - %ld\n", i, (i * (n / in_socket_partitions)), ((i+1) * (n / in_socket_partitions)));
    }
    __gnu_parallel::sort(a + ((in_socket_partitions-1) * (n / in_socket_partitions)), a + n);
    partition_starts[in_socket_partitions-1] = ((in_socket_partitions-1) * (n / in_socket_partitions));
    partition_sizes[in_socket_partitions-1] = n - ((in_socket_partitions-1) * (n / in_socket_partitions));
    printf("partition %ld: %ld - %ld\n", in_socket_partitions-1, ((in_socket_partitions-1) * (n / in_socket_partitions)), n);
   
    
    //printArray2(a, n, 0);
    
    //printArray2(x, (n/2), 0);
    pthread_barrier_t *barrier11 = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    pthread_barrier_t *barrier2 = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier11, NULL, threads);
    pthread_barrier_init(barrier2, NULL, threads+1);
    help_array_a = (long*)malloc(threads * sizeof(long));
    help_array_b = (long*)malloc(threads * sizeof(long));
    //printArray2(a, n/2, 0);
    //printArray2(x, n/2, 0);
    pthread_t* the_threads = (pthread_t*)malloc(threads * sizeof(pthread_t));
    //===============
    for(i=0;i<threads;i++) {
        merge_args_t *merge_args = (merge_args_t *)malloc(sizeof(merge_args_t));
        merge_args->src = a;
        merge_args->nthreads = threads;
        merge_args->threads_per_partition = threads_per_partition;
        merge_args->partitions = in_socket_partitions;
        merge_args->partition_starts = partition_starts;
        merge_args->partition_sizes = partition_sizes;
        merge_args->round_barrier = barrier11;
        merge_args->dest = res;
        merge_args->dest_ptr = &res;
        merge_args->sizea = n;
        merge_args->alloc = alloc;
        pthread_create(&the_threads[i], NULL, merge, merge_args);
    }
    gettimeofday(&start, NULL);
    for(i=0;i<threads;i++) {
      pthread_join(the_threads[i], NULL);
    }

    gettimeofday(&stop, NULL);

    usec = (stop.tv_sec - start.tv_sec) * 1e6;
    usec += (stop.tv_usec - start.tv_usec);
    //printf("%d,%d,%.2f\n",n,threads,usec/(double)1e3);
    //printf("%ld, duration            : %d (ms)\n", n, (int)(usec/(double)1e3));
    printf("duration            : %d (ms)\n", (int)(usec/(double)1e3));
    //printArray2(a, n, 0);
    //printArray2(res, n, 0);
    for (long i = 0; i < in_socket_partitions; i+=2){
      //is_sorted(res + (i * (n / (in_socket_partitions / 2) )), (n / (in_socket_partitions / 2)));
      //printf("partition %ld: %ld - %ld is sorted\n", i, (i * (n / in_socket_partitions)), ((i+1) * (n / in_socket_partitions)));
    }
    printArray2(res, n, 1);
    return 0;
}

void is_sorted(SORT_TYPE *y, const long n)
{
    long i;
    SORT_TYPE min = y[0];
    for(i = 1; i < n; i++){
          if (y[i] < min) {
            fprintf(stderr, "Error for i = %ld\n", i); fflush(stderr);
            assert(0);
          }
          min = y[i];
    }
}

void readArray2(SORT_TYPE a[], const long limit)
{
    long i;
    printf("Populating the array...");fflush(stdout);
    for (i=0; i<limit; i++){
        a[i]=i;
    }

    // Knuth shuffle
    for (i=limit-1; i > 0; i--){
        const uint j = mrand(seeds) % limit;
        const int tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
     }
    printf("Done\n");fflush(stdout);
}


void printArray2(SORT_TYPE *y, const long n, const int assert)
{
    long i;
    for(i = 0; i < n; i++){
        if (assert) {
          if (y[i] != (SORT_TYPE)i) {
            fprintf(stderr, "Error for i = %ld\n", i); fflush(stderr);
            assert(0);
          }
        }
        else
          printf("x[%ld] = %u, ",i,y[i]);
    }
    printf("\n");
}

