#ifndef __H_MCTOP_RAND__
#define __H_MCTOP_RAND__

#include <mctop.h>

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


  static inline void mctop_get_knuth_array_uint(uint *array, size_t length, unsigned long* seeds)
  {
    char fname[128];
    sprintf(fname, "data/mctop_sort_knuth_%zu.dat", length);
    FILE* rand_file = fopen(fname, "r");

    if (rand_file == NULL)
      {
        rand_file = fopen(fname, "w+");
        assert(rand_file != NULL);
        for (size_t i = 0; i < length; i++) {
          array[i] = i;
        }
        for (size_t i = length - 1; i > 0; i--) {
          const uint j = mctop_rand(seeds) % length;
          const uint tmp = array[i];
          array[i] = array[j];
          array[j] = tmp;
        }
        for (size_t i = 0; i < length; i++) {
          fprintf(rand_file, "%d\n", array[i]);
        }
      }
    else
      {
        printf("  // reading from file..\n");
        for (size_t i = 0; i < length; i++) {
          uint val;
          __attribute__((unused)) int ret = fscanf(rand_file, "%u", &val);
          array[i] = val;
        }
      }
    fclose(rand_file);
  }

  static inline void mctop_get_random_array_uint(uint *array, size_t length, unsigned long* seeds)
  {
    char fname[128];
    sprintf(fname, "data/mctop_sort_random_%zu.dat", length);
    FILE* rand_file = fopen(fname, "r");

    if (rand_file == NULL)
      {
        rand_file = fopen(fname, "w+");
        assert(rand_file != NULL);
        for (size_t i = 0; i < length; i++) {
          array[i] = mctop_rand(seeds) % (2000000000);
          fprintf(rand_file, "%d\n", array[i]);
        }
      }
    else
      {
        for (size_t i = 0; i < length; i++) {
          uint val;
          __attribute__((unused)) int ret = fscanf(rand_file, "%u", &val);
          array[i] = val;
        }
      }
    fclose(rand_file);
  }


  static inline void mctop_get_sorted_array_uint(uint *array, size_t length)
  {
    char fname[128];
    sprintf(fname, "data/mctop_sort_sorted_%zu.dat", length);
    FILE* rand_file = fopen(fname, "r");

    if (rand_file == NULL)
      {
        rand_file = fopen(fname, "w+");
        assert(rand_file != NULL);
        for (size_t i = 0; i < length; i++) {
          array[i] = i;
          fprintf(rand_file, "%d\n", array[i]);
        }
      }
    else
      {
        for (size_t i = 0; i < length; i++) {
          uint val;
          __attribute__((unused)) int ret = fscanf(rand_file, "%u", &val);
          array[i] = val;
        }
      }
    fclose(rand_file);
  }


  static inline void mctop_get_sorted_endswitched_array_uint(uint *array, size_t length)
  {
    char fname[128];
    sprintf(fname, "data/mctop_sort_endswitched_%zu.dat", length);
    FILE* rand_file = fopen(fname, "r");

    if (rand_file == NULL)
      {
        rand_file = fopen(fname, "w+");
        assert(rand_file != NULL);
        for (size_t i = 0; i < length; i++) {
          array[i] = i;
        }
        
        uint tmp = array[0];
        array[0] = array[length-1];
        array[length-1] = tmp;

        for (size_t i = 0; i < length; i++) {
          fprintf(rand_file, "%d\n", array[i]);
        }
      }
    else
      {
        for (size_t i = 0; i < length; i++) {
          uint val;
          __attribute__((unused)) int ret = fscanf(rand_file, "%u", &val);
          array[i] = val;
        }
      }
    fclose(rand_file);
  }


  static inline void mctop_get_reversesorted_array_uint(uint *array, size_t length)
  {
    char fname[128];
    sprintf(fname, "data/mctop_sort_reversesorted_%zu.dat", length);
    FILE* rand_file = fopen(fname, "r");

    if (rand_file == NULL)
      {
        rand_file = fopen(fname, "w+");
        assert(rand_file != NULL);
        for (size_t i = 0; i < length; i++) {
          array[i] = length - 1 - i;
          fprintf(rand_file, "%d\n", array[i]);
        }
      }
    else
      {
        for (size_t i = 0; i < length; i++) {
          uint val;
          __attribute__((unused)) int ret = fscanf(rand_file, "%u", &val);
          array[i] = val;
        }
      }
    fclose(rand_file);
  }

#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_RAND__ */

