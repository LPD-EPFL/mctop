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

//pthread_mutex_t global_lock;

void readArray2(SORT_TYPE [], const long);
void printArray2(SORT_TYPE [], const long, const int);

SORT_TYPE *a __attribute__((aligned(64)));
SORT_TYPE *res __attribute__((aligned(64)));

long *help_array_a __attribute__((aligned(64)));
long *help_array_b __attribute__((aligned(64)));

long in_socket_partitions;
long threads_per_partition;
unsigned long* seeds;

//partition merge arguments
typedef struct merge_args_t
{
    SORT_TYPE* src;
    long sizea;
    SORT_TYPE* dest;
    long partitions;
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



void *merge(void *args) {
  merge_args_t *myargs = (merge_args_t *)args;

  mctop_alloc_pin(myargs->alloc);

  long my_id = mctop_alloc_thread_core_insocket_id();
  
  long next_merge, partition_size, pos_in_merge, partition1, partition_a_start, partition_a_size, partition_b_start, partition_b_size;
  
  next_merge = my_id / myargs->threads_per_partition;
  if (next_merge % 2) next_merge++;
  while (1) {
    partition_size = myargs->sizea / myargs->partitions;

    pos_in_merge = my_id % myargs->threads_per_partition;

    
    partition1 = next_merge;
    if (next_merge >= myargs->partitions)
      break;

    partition_a_start =  partition1 * partition_size;
    partition_a_size = partition_size;
    partition_b_start = (partition1 + 1) * partition_size;
    partition_b_size = partition_size;

    //printf("[thread %d] getting to work on merge %ld, partition_1 %ld partition_2 %ld, size_a %ld, size_b %ld, dest %ld pos_in_merge %ld\n", mctop_alloc_thread_core_insocket_id(), next_merge, partition_a_start, partition_b_start, partition_a_size, partition_b_size, partition_a_start, pos_in_merge); fflush(stdout);
    SORT_TYPE *my_a = &myargs->src[partition_a_start];
    SORT_TYPE *my_b = &myargs->src[partition_b_start];
    SORT_TYPE *my_dest = &myargs->dest[partition_a_start];

    merge_arrays(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
    next_merge += (myargs->nthreads / myargs->threads_per_partition) * 2;
  }
  return NULL;
}



/*

void merge(SORT_TYPE *a, SORT_TYPE *dest, long sizea, long sizeb, long nthreads, long npartitions, long thread_per_partition){
  long i,j,k;
  long partition_nexg= 0;

  long partition_size = sizea / npartitions;

 
  while (partition_next < npartitions) {
    for (i=0; i < nthreads; i+=threads_per_partition) {
      //merge using threads_per_partition
      partition_a_start = partition_next * partition_size;
      partition_a_size = partition_size;
      partition_b_start = (partitions_next + 1) * partition_size;
      partition_b_size = partition_size;
      the_thread = threads[i];
      merge_args_t *args = merge_args[i];
      args->i = i % threads_per_partition;
      args->barrier_start1 = barrier11;
      args->barrier_end = barrier2;
      args->nthreads = threads_per_partition;
      args->a = a[partition_a_start];
      args->b = a[partition_b_start];
      args->help_array_b = help_array_b;
      args->help_array_a = help_array_a;
      args->dest = res;
      args->sizea = partition_size;
      args->sizeb = partition_size;
      args->alloc = alloc;
      create(the_thread, NULL, merge, merge_args);
    }
  }
}
*/
int main(int argc,char *argv[]){
    long n, i, threads;
    struct timeval start, stop;
    unsigned long usec;
    mctop_alloc_policy allocation_policy;
    assert(argc >= 4);
    seeds = seed_rand();
    long array_size_mb = atol(argv[1]);

    n = array_size_mb * 1024 * (1024 / sizeof(SORT_TYPE));
    n = n & 0xFFFFFFFFFFFFFFF0L;
    
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
    pthread_barrier_init(barrier11, NULL, threads+1);
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
        merge_args->dest = res;
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
    //printArray2(res, n, 0);
    //printArray2(res, n, 1);
    return 0;
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
          printf("x[%ld] = %u\n",i,y[i]);
    }
    printf("\n");
}

