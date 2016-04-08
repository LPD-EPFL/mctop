#ifndef __H_MCTOP_RAND__
#define __H_MCTOP_RAND__

#ifdef __cplusplus
extern "C" {
#endif

  static inline unsigned long* 
  seed_rand_fixed() 
  {
    unsigned long* seeds;
    seeds = (unsigned long*) malloc(64);
    seeds[0] = 1;
    seeds[1] = 2;
    seeds[2] = 3;
    return seeds;
  }

  static inline unsigned long* 
  seed_rand() 
  {
    unsigned long* seeds;
    seeds = (unsigned long*) malloc(64);
    seeds[0] = mctop_getticks();
    seeds[1] = mctop_getticks() & 0x10101010EEF;
    seeds[2] = mctop_getticks() * 0xFAB0BAF0;
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

  static inline unsigned long
  mctop_rand(unsigned long* seeds)
  {
    return (xorshf96(seeds, seeds + 1, seeds + 2));
  }


#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_RAND__ */

