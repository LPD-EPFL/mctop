#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <algorithm>

#define RAND_RANGE(N) ((double)rand() / ((double)RAND_MAX + 1) * (N))
#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)

//pthread_mutex_t global_lock;

void readArray2(uint64_t [], const long);
void printArray2(uint64_t [], const long, const int);

uint64_t *a __attribute__((aligned(64)));


int main(int argc,char *argv[]){
    long n, threads;
    struct timeval start, stop;
    unsigned long usec;
    assert(argc >= 4);

    long array_size_mb = atol(argv[1]);

    n = array_size_mb * 1024 * 1024 / sizeof(uint);
    n = n & 0xFFFFFFFFFFFFFFF0L;
    
    assert (n%4==0);
    a = (uint64_t*) malloc(n*sizeof(uint64_t));

    threads = atoi(argv[2]);
    assert(threads>0);
    assert(!(threads & (threads-1)));

    if (argc > 4)
      srand(atoi(argv[4]));
    else
      srand(42);

    readArray2(a, n);

    gettimeofday(&start, NULL);
    //===============
    std::sort(a, a+n);
    //==============
    gettimeofday(&stop, NULL);

    usec = (stop.tv_sec - start.tv_sec) * 1e6;
    usec += (stop.tv_usec - start.tv_usec);
    //printf("%d,%d,%.2f\n",n,threads,usec/(double)1e3);
    printf("duration            : %d (ms)\n", (int)(usec/(double)1e3));
    printArray2(a, n, 1);
    return 0;
}


void readArray2(uint64_t a[], const long limit)
{
    long i;
    printf("Populating the array...");fflush(stdout);
    for (i=0; i<limit; i++){
        a[i]=i;
    }

    // Knuth shuffle
    for (i=limit-1; i > 0; i--){
        long j = RAND_RANGE(i);
        uint64_t tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
     }
    printf("Done\n");fflush(stdout);
}


void printArray2(uint64_t *y, const long n, const int assert)
{
    long i;
    for(i = 0; i < n; i++){
        if (assert)
          assert(y[i] == (uint64_t)i);
        else
          printf("x[%ld] = %lu \n",i,y[i]);
    }
}
