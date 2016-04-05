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
#include <mctop.h>
#include <nmmintrin.h>

#define RAND_RANGE(N) ((double)rand() / ((double)RAND_MAX + 1) * (N))
#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)

//pthread_mutex_t global_lock;

void readArray2(uint [], const long);
void printArray2(uint [], const long, const int);

uint *a __attribute__((aligned(64)));
uint *x __attribute__((aligned(64)));
uint *res __attribute__((aligned(64)));

long *indicesa __attribute__((aligned(64)));
long *indicesb __attribute__((aligned(64)));
long *res2 __attribute__((aligned(64)));


//partition merge arguments
typedef struct merge_args_t
{
    uint* a;
    uint sizea;
    uint* b;
    uint sizeb;
    uint* dest;
    int i;
    long *indicesa;
    long *indicesb;
    long *res2;
    int node;
    int nthreads;
    pthread_barrier_t *barrier_start1;
    pthread_barrier_t *barrier_start2;
    pthread_barrier_t *barrier_end;
    mctop_alloc_t *alloc;
} merge_args_t;


//4-wide bitonic merge network
inline void bitonic_merge(__m128 a, __m128 b, __m128* res_lo, __m128* res_hi) {
    b = _mm_shuffle_ps(b,b,_MM_SHUFFLE(0,1,2,3));
    __m128 l1 = _mm_min_ps(a,b);
    __m128 h1 = _mm_max_ps(a,b);
    __m128 l1p = _mm_shuffle_ps(l1,h1,_MM_SHUFFLE(1,0,1,0));
    __m128 h1p = _mm_shuffle_ps(l1,h1,_MM_SHUFFLE(3,2,3,2));
    __m128 l2 = _mm_min_ps(l1p, h1p);
    __m128 h2 = _mm_max_ps(l1p, h1p);
    __m128 l2u = _mm_unpacklo_ps(l2,h2);
    __m128 h2u = _mm_unpackhi_ps(l2,h2);
    __m128 l2p = _mm_shuffle_ps(l2u,h2u,_MM_SHUFFLE(1,0,1,0));
    __m128 h2p = _mm_shuffle_ps(l2u,h2u,_MM_SHUFFLE(3,2,3,2));
    __m128 l3 = _mm_min_ps(l2p,h2p);
    __m128 h3 = _mm_max_ps(l2p,h2p);
    *res_lo = _mm_unpacklo_ps(l3,h3);
    *res_hi = _mm_unpackhi_ps(l3,h3);
}


//binary search for a value in a vector
long bs(uint* data, uint size, uint value) {
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

void merge_serial(uint* a, uint* b, uint* dest, uint num_a, uint num_b) {
    uint i,j;
    i=0;
    j=0;
    int k=0;

    while ((i<num_a) && (j<num_b))
    {
        if (a[i] < b[j])
            dest[k++] = a[i++];

        else
            dest[k++] = b[j++];
    }

    while (i < num_a)
        dest[k++] = a[i++];

    while (j < num_b)
        dest[k++] = b[j++];
}

void merge_do(uint* a, uint* b, uint* dest, uint num_a, uint num_b) {
  //first take care of portions flowing over the 16-byte boundaries
    long i,j,k;
   

    i=0;
    j=0;
    k=0;

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
    //printf("[thread %ld] &a[i] = %ld &b[j] = %ld\n", pthread_self(), (uintptr_t) &a[i], (uintptr_t) &b[j]);
    //printf("[thread %ld] num_a = %ld, num_b = %ld got alignment after %ld steps i = %ld, j = %ld\n", pthread_self(), num_a, num_b, k, i, j);
    long sizea = num_a - i;
    long sizeb = num_b - j;
    //assert((sizea % 4) == 0);
    //assert((sizeb % 4) == 0);
    long size_a_128 = sizea / 4;
    long size_b_128 = sizeb / 4;


    __m128* a128 = (__m128*) &a[i];
    __m128* b128 = (__m128*) &b[j];
    __m128* dest128 = (__m128*) &dest[k];

    uint crt_a = 0;
    uint crt_b = 0;
    uint next_val = 0;

    __m128 next;
    __m128 last;

    if (_mm_comilt_ss(*a128,*b128)){
        next = *b128;
        crt_b++;
    } else {
        next = *a128;
        crt_a++;
    }

    while ((crt_a < size_a_128) || (crt_b < size_b_128)){
        if ((crt_a < size_a_128) && ((crt_b>=size_b_128) || (_mm_comilt_ss(*(a128 + crt_a),*(b128 + crt_b))))){
            bitonic_merge(next,a128[crt_a],&(dest128[next_val]),&last);
            crt_a++;
        } else {
            bitonic_merge(next,b128[crt_b],&(dest128[next_val]),&last);
            crt_b++;
        }
        next_val++;
        next=last;
    }
    *(dest128+next_val)=next;
    i += size_a_128;
    j += size_b_128;
    //printf("[thread %ld] num_a = %ld, num_b = %ld k = %ld i = %ld, j = %ld\n", pthread_self(), num_a, num_b, k, i, j);
    k = i+j;

    while((i<num_a) && (j<num_b)) {
        if (a[i] < b[j])
            dest[k++] = a[i++];

        else
            dest[k++] = b[j++];
    }

    while (i < num_a)
        dest[k++] = a[i++];

    while (j < num_b)
        dest[k++] = b[j++];
}

void *merge(void *args) {
  merge_args_t *myargs = (merge_args_t *)args;
  long res1;

  mctop_alloc_pin(myargs->alloc);
  //numa_run_on_node(myargs->node);
  long size1, size2, desti, i;
  long a = 0, aprime = 0, b = 0, bprime = 0;
  if (myargs->i > 0) {
  long gamma = 0 + ((myargs->i) * (myargs->sizea + myargs->sizeb) / myargs->nthreads);
  long amin = max(0, (0+gamma-0-(myargs->sizeb+1)));
  long amax = min(myargs->sizea+1, 0+gamma-0);
  long aminprime = amin;
  long amaxprime = amax;
  long length = gamma - 0 + 0 + 0;
  while ((aminprime + 1) != amaxprime) {
    aprime = (aminprime + amaxprime) / 2;
    bprime = length - aprime;
    if (myargs->a[aprime] <= myargs->b[bprime])
      aminprime = aprime;
    else
      amaxprime = aprime;
  }
  if (myargs->a[aminprime] <= myargs->b[length-amaxprime])
    a = amaxprime;
  else
    a = aminprime;
  b = length - a;
  }

  myargs->indicesa[myargs->i] = a;
  myargs->indicesb[myargs->i] = b;
  
  pthread_barrier_wait(myargs->barrier_start1);
  if (myargs->i < myargs->nthreads - 1) {
    size1 = myargs->indicesa[myargs->i+1] - myargs->indicesa[myargs->i];
    size2 = myargs->indicesb[myargs->i+1] - myargs->indicesb[myargs->i];
  }
  else {
    size1 = myargs->sizea - myargs->indicesa[myargs->i];
    size2 = myargs->sizeb - myargs->indicesb[myargs->i];
  }
  desti = a+b;
  
  //printf("[thread %ld / %d] res1 %ld res2 %ld desti = %ld size1 = %ld size2 = %ld &a[res1] = %ld &b[res2] = %ld\n", pthread_self(), myargs->i, a, b, desti, size1, size2, (uintptr_t) &myargs->a[a], (uintptr_t) &myargs->b[b]);
  pthread_barrier_wait(myargs->barrier_start2);
  merge_do(&myargs->a[a], &myargs->b[b], &myargs->dest[desti], size1, size2);
  pthread_barrier_wait(myargs->barrier_end);
  return NULL;
}

void merge_sse(uint* aa, uint* bb, uint* dest_u, uint na, uint nb){
    assert(na % 4 == 0 && nb%4 == 0);
    //now use sse to merge
    __m128* a = (__m128*) aa;
    __m128* b = (__m128*) bb;
    __m128* dest = (__m128*) dest_u;
    uint num_a = na/4;
    uint num_b = nb/4;

    uint crt_a = 0;
    uint crt_b = 0;
    uint next_val = 0;

    __m128 next;
    __m128 last;

    if (_mm_comilt_ss(*a,*b)){
        next = *b;
        crt_b++;
    } else {
        next = *a;
        crt_a++;
    }

    while ((crt_a < num_a) || (crt_b < num_b)){
        if ((crt_a < num_a) && ((crt_b>=num_b) || (_mm_comilt_ss(*(a + crt_a),*(b + crt_b))))){
            bitonic_merge(next,a[crt_a],&(dest[next_val]),&last);
            crt_a++;
        } else {
            bitonic_merge(next,b[crt_b],&(dest[next_val]),&last);
            crt_b++;
        }
        next_val++;
        next=last;
    }
    *(dest+next_val)=next;
}

int main(int argc,char *argv[]){
    long n, i, threads;
    struct timeval start, stop;
    unsigned long usec;
    mctop_alloc_policy allocation_policy;
    assert(argc >= 4);

    long array_size_mb = atol(argv[1]);

    n = array_size_mb * 1024 * (1024 / sizeof(uint));
    n = n & 0xFFFFFFFFFFFFFFF0L;
    
    assert (n%4==0);
    a = (uint*) numa_alloc_onnode(n*sizeof(uint), 0);
    x = (uint*) numa_alloc_onnode(n*sizeof(uint), 0);
    res = (uint*) numa_alloc_onnode(n*sizeof(uint), 0);

    threads = atoi(argv[2]);
    assert(threads>0);

    if (argc > 4)
      srand(atoi(argv[4]));
    else
      srand(42);


    allocation_policy = (mctop_alloc_policy) atoi(argv[3]);
    mctop_t * topo = mctop_load(NULL);
    mctop_alloc_t *alloc = mctop_alloc_create(topo, threads, MCTOP_ALLOC_ALL, allocation_policy);
    readArray2(a, n);
    memcpy((void*)x,(void*)&a[n/2],(n/2) * sizeof(uint));
    __gnu_parallel::sort(a, a+(n/2));
    __gnu_parallel::sort(x, x+(n/2));
    //printArray2(a, (n/2), 0);
    //printArray2(x, (n/2), 0);
    pthread_barrier_t *barrier11 = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    pthread_barrier_t *barrier12 = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    pthread_barrier_t *barrier2 = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier11, NULL, threads+1);
    pthread_barrier_init(barrier12, NULL, threads+1);
    pthread_barrier_init(barrier2, NULL, threads+1);
    indicesa = (long*)malloc(threads * sizeof(long));
    indicesb = (long*)malloc(threads * sizeof(long));
    res2 = (long*)malloc(threads * sizeof(long));
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
        merge_args->barrier_start2 = barrier12;
        merge_args->barrier_end = barrier2;
        merge_args->nthreads = threads;
        merge_args->a = a;
        merge_args->b = x;
        merge_args->indicesb = indicesb;
        merge_args->indicesa = indicesa;
        merge_args->res2 = res2;
        merge_args->dest = res;
        merge_args->sizea = n/2;
        merge_args->sizeb = n/2;
        merge_args->alloc = alloc;
        pthread_create(the_thread, NULL, merge, merge_args);
    }
    pthread_barrier_wait(barrier11);
    pthread_barrier_wait(barrier12);
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
        if (assert) {
          if (y[i] != (uint)i) {
            fprintf(stderr, "Error for i = %ld\n", i); fflush(stderr);
            assert(0);
          }
        }
        else
          printf("x[%ld] = %u\n",i,y[i]);
    }
    printf("\n");
}

