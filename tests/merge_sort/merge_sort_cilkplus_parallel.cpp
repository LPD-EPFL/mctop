#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <cilk/cilk.h>
#include "mctop_rand.h"
#include "mctop_sort.h"

#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)

//pthread_mutex_t global_lock;

void readArray2(MCTOP_SORT_TYPE [], const long);
void printArray2(MCTOP_SORT_TYPE [], const long, const int);

MCTOP_SORT_TYPE *a __attribute__((aligned(64)));
unsigned long *seeds;

int main(int argc,char *argv[]){
    long n, threads;
    struct timeval start, stop;
    unsigned long usec;
    assert(argc >= 3);

    long array_size_mb = atol(argv[1]);
    seeds = seed_rand_fixed();
    n = array_size_mb * 1024LL * (1024LL / sizeof(MCTOP_SORT_TYPE));
    
    a = (MCTOP_SORT_TYPE*) malloc(n*sizeof(MCTOP_SORT_TYPE));

    threads = atoi(argv[2]);
    assert(threads>0);

    readArray2(a, n);

    gettimeofday(&start, NULL);
    //===============
    cilkpub::cilk_sort(a, a+n);
    //==============
    gettimeofday(&stop, NULL);

    usec = (stop.tv_sec - start.tv_sec) * 1e6;
    usec += (stop.tv_usec - start.tv_usec);
    //printf("%d,%d,%.2f\n",n,threads,usec/(double)1e3);
    printf("duration            : %d (ms)\n", (int)(usec/(double)1e3));
    printArray2(a, n, 1);
    return 0;
}


void readArray2(MCTOP_SORT_TYPE a[], const long limit)
{
    long i;
    printf("Populating the array...");fflush(stdout);
    for (i=0; i<limit; i++){
        a[i]=i;
    }

    // Knuth shuffle
    for (i=limit-1; i > 0; i--){
        long j = mctop_rand(seeds) % limit;
        MCTOP_SORT_TYPE tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
     }
    printf("Done\n");fflush(stdout);
}


void printArray2(MCTOP_SORT_TYPE *y, const long n, const int assert)
{
    long i;
    for(i = 0; i < n; i++){
        if (assert)
          assert(y[i] == (MCTOP_SORT_TYPE)i);
        else
          printf("x[%ld] = " MCTOP_SORT_TYPE_FORMAT " \n",i,y[i]);
    }
}
