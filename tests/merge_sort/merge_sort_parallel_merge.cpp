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
#include <nmmintrin.h>
#include "merge_utils.h"

#define RAND_RANGE(N) ((double)rand() / ((double)RAND_MAX + 1) * (N))
#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)



//pthread_mutex_t global_lock;

void readArray2(SORT_TYPE [], const long);
void printArray2(SORT_TYPE [], const long, const int);

SORT_TYPE *a __attribute__((aligned(64)));
SORT_TYPE *x __attribute__((aligned(64)));
SORT_TYPE *res __attribute__((aligned(64)));

long *help_array_a __attribute__((aligned(64)));
long *help_array_b __attribute__((aligned(64)));

long in_thread_partitions;


//partition merge arguments
typedef struct merge_args_t
{
    SORT_TYPE* a;
    SORT_TYPE sizea;
    SORT_TYPE* b;
    SORT_TYPE sizeb;
    SORT_TYPE* dest;
    int i;
    long *help_array_a;
    long *help_array_b;
    int node;
    int nthreads;
    pthread_barrier_t *barrier_start1;
    pthread_barrier_t *barrier_end;
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
  //numa_run_on_node(myargs->node);

  merge_arrays(myargs->a, myargs->b, myargs->dest, myargs->sizea, myargs->sizeb, myargs->i, myargs->nthreads);
  pthread_barrier_wait(myargs->barrier_end);
  return NULL;
}





void merge(SORT_TYPE *a, SORT_TYPE *b, SORT_TYPE *dest, long sizea, long sizeb){




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
    
    assert (n%4==0);
    a = (SORT_TYPE*) numa_alloc_onnode(n*sizeof(SORT_TYPE), 0);
    x = (SORT_TYPE*) numa_alloc_onnode(n*sizeof(SORT_TYPE), 0);
    res = (SORT_TYPE*) numa_alloc_onnode(n*sizeof(SORT_TYPE), 0);

    threads = atoi(argv[2]);
    assert(threads>0);

    if (argc > 4)
      srand(atoi(argv[4]));
    else
      srand(42);

    if (argc > 5)
      in_thread_partitions = atoi(argv[5]);
    else
      in_thread_partitions = threads;


    allocation_policy = (mctop_alloc_policy) atoi(argv[3]);
    mctop_t * topo = mctop_load(NULL);
    mctop_alloc_t *alloc = mctop_alloc_create(topo, threads, MCTOP_ALLOC_ALL, allocation_policy);
    mctop_alloc_print_short(alloc);

    readArray2(a, n);
    memcpy((void*)x,(void*)&a[n/2],(n/2) * sizeof(SORT_TYPE));
    __gnu_parallel::sort(a, a+(n/2));
    __gnu_parallel::sort(x, x+(n/2));
    //printArray2(a, (n/2), 0);
    //printArray2(x, (n/2), 0);
    pthread_barrier_t *barrier11 = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    pthread_barrier_t *barrier2 = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier11, NULL, threads+1);
    pthread_barrier_init(barrier2, NULL, threads+1);
    help_array_a = (long*)malloc(threads * sizeof(long));
    help_array_b = (long*)malloc(threads * sizeof(long));
    //printArray2(a, n/2, 0);
    //printArray2(x, n/2, 0);
    gettimeofday(&start, NULL);
    //===============
    for(i=0;i<threads;i++) {
        pthread_t* the_thread = (pthread_t*)malloc(sizeof(pthread_t));
        merge_args_t *merge_args = (merge_args_t *)malloc(sizeof(merge_args_t));
        merge_args->i = i;
        merge_args->node = i%2;
        merge_args->barrier_start1 = barrier11;
        merge_args->barrier_end = barrier2;
        merge_args->nthreads = threads;
        merge_args->a = a;
        merge_args->b = x;
        merge_args->help_array_b = help_array_b;
        merge_args->help_array_a = help_array_a;
        merge_args->dest = res;
        merge_args->sizea = n/2;
        merge_args->sizeb = n/2;
        merge_args->alloc = alloc;
        pthread_create(the_thread, NULL, merge, merge_args);
    }
    pthread_barrier_wait(barrier2);

    gettimeofday(&stop, NULL);

    usec = (stop.tv_sec - start.tv_sec) * 1e6;
    usec += (stop.tv_usec - start.tv_usec);
    //printf("%d,%d,%.2f\n",n,threads,usec/(double)1e3);
    printf("%ld, duration            : %d (ms)\n", n, (int)(usec/(double)1e3));
    //printArray2(res, n, 0);
    printArray2(res, n, 1);
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
          printf("x[%ld] = %u\n",i,y[i]);
    }
    printf("\n");
}

