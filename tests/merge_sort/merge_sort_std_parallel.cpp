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

#define RAND_RANGE(N) ((double)rand() / ((double)RAND_MAX + 1) * (N))
#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)

//pthread_mutex_t global_lock;

void readArray2(uint [], const long);
void printArray2(uint [], const long, const int);

uint *a __attribute__((aligned(64)));


int main(int argc,char *argv[]){
    long n, threads;
    struct timeval start, stop;
    unsigned long usec;
    assert(argc >= 4);

    long array_size_mb = atol(argv[1]);

    n = array_size_mb * 1024 * 1024 / sizeof(uint);
    n = n & 0xFFFFFFFFFFFFFFF0L;
    
    assert (n%4==0);
    a = (uint*) malloc(n*sizeof(uint));

    threads = atoi(argv[2]);
    assert(threads>0);

    if (argc > 4)
      srand(atoi(argv[4]));
    else
      srand(42);

    readArray2(a, n);

    gettimeofday(&start, NULL);
    //===============
    omp_set_num_threads(threads);
    __gnu_parallel::sort(a, a+n);
    //==============
    gettimeofday(&stop, NULL);

    usec = (stop.tv_sec - start.tv_sec) * 1e6;
    usec += (stop.tv_usec - start.tv_usec);
    //printf("%d,%d,%.2f\n",n,threads,usec/(double)1e3);
    printf("duration            : %d (ms)\n", (int)(usec/(double)1e3));
    printArray2(a, n, 1);
    return 0;
}


void readArray2(uint a[], const long limit)
{
    long i;
    printf("Populating the array...");fflush(stdout);
    for (i=0; i<limit; i++){
        a[i]=i;
    }

    // Knuth shuffle
    for (i=limit-1; i > 0; i--){
        long j = RAND_RANGE(i);
        uint tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
     }
    printf("Done\n");fflush(stdout);
}


void printArray2(uint *y, const long n, const int assert)
{
    long i;
    for(i = 0; i < n; i++){
        if (assert)
          assert(y[i] == (uint)i);
        else
          printf("x[%ld] = %lu \n",i,y[i]);
    }
}
