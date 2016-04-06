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




void merge(SORT_TYPE *src, SORT_TYPE *dest, long sizea, long partitions, long threads_per_partition, long nthreads) {

  long my_id = mctop_alloc_thread_core_insocket_id();
  
  long next_merge, partition_size, pos_in_merge, partition1, partition_a_start, partition_a_size, partition_b_start, partition_b_size;
  
  next_merge = my_id / threads_per_partition;
  if (next_merge % 2) next_merge++;
  while (1) {
    partition_size = sizea / partitions;

    pos_in_merge = my_id % threads_per_partition;

    
    partition1 = next_merge;
    if (next_merge >= partitions)
      break;

    partition_a_start =  partition1 * partition_size;
    partition_a_size = partition_size;
    partition_b_start = (partition1 + 1) * partition_size;
    partition_b_size = partition_size;

    //printf("[thread %d] getting to work on merge %ld, partition_1 %ld partition_2 %ld, size_a %ld, size_b %ld, dest %ld pos_in_merge %ld\n", mctop_alloc_thread_core_insocket_id(), next_merge, partition_a_start, partition_b_start, partition_a_size, partition_b_size, partition_a_start, pos_in_merge); fflush(stdout);
    SORT_TYPE *my_a = &src[partition_a_start];
    SORT_TYPE *my_b = &src[partition_b_start];
    SORT_TYPE *my_dest = &dest[partition_a_start];

    merge_arrays(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
    next_merge += (nthreads / threads_per_partition) * 2;
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
    if (mctop_alloc_thread_core_insocket_id() == 0)
      printf("doing round %ld\n", partitions); 
    merge(src, dest, size, partitions, threads_per_partition, nthreads);
    partitions = partitions / 2;
    pthread_barrier_wait(myargs->round_barrier);
    if (mctop_alloc_thread_core_insocket_id() == 0) {
      //printArray2(dest, size, 0);
    }
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
    assert(argc >= 4);

    long array_size_mb = atol(argv[1]);

    n = array_size_mb * 1024 * (1024 / sizeof(SORT_TYPE));
    n = n & 0xFFFFFFFFFFFFFFF0L;
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

    // sort in partitions
    assert((n % in_socket_partitions) == 0);
    for (long i = 0; i < in_socket_partitions; i++){
      __gnu_parallel::sort(a + (i * (n / in_socket_partitions)), a + ((i+1) * (n / in_socket_partitions)));
      //printf("partition %ld: %ld - %ld\n", i, (i * (n / in_socket_partitions)), ((i+1) * (n / in_socket_partitions)));
    }
   
    
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
        long j = RAND_RANGE(i);
        SORT_TYPE tmp = a[i];
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

