#ifndef __H_MERGE_SSE_UTILS__
#define __H_MERGE_SSE_UTILS__

#include <nmmintrin.h>

//4-wide bitonic merge network
static inline void sse_bitonic_merge_4(__m128 a, __m128 b, __m128* res_lo, __m128* res_hi) {
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


static inline void sse_merge_aligned_32bit(void *a, void *b, void *dest, size_t size_a, size_t size_b) {
  size_t size_a_128 = size_a >> 2;
  size_t size_b_128 = size_b >> 2;
  
  __m128* a128 = (__m128*) a;
  __m128* b128 = (__m128*) b;
  __m128* dest128 = (__m128*) dest;
  
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

  const size_t size_ab = size_a_128 + size_b_128;
  for (size_t ii = (crt_a + crt_b); ii < size_ab; ii++)
    {
     // while ((crt_a < size_a_128) || (crt_b < size_b_128)){
      if ((crt_a < size_a_128) && ((crt_b>=size_b_128) || (_mm_comilt_ss(*(a128 + crt_a),*(b128 + crt_b))))){
          sse_bitonic_merge_4(next,a128[crt_a],&(dest128[next_val]),&last);
          crt_a++;
      } else {
          sse_bitonic_merge_4(next,b128[crt_b],&(dest128[next_val]),&last);
          crt_b++;
      }
      next_val++;
      next=last;
  }
  *(dest128+next_val)=next;
} 

#endif
