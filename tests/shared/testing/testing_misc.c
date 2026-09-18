// Copyright 2021-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
// XMOS Public License: Version 1

#include "testing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef __xcore__
 #include "xcore/hwtimer.h"
#endif


unsigned get_seed(
    const char* str, 
    const unsigned len)
{
  unsigned seed = 0;
  unsigned left = len;


  // don't read past the end of the buffer
  while(left >= 4){
    unsigned v;
    memcpy(&v, str, sizeof(v));
    seed = seed ^ v;
    left -= 4;
    str = &str[4];
  }

  // read the remaining bytes (if any) and xor them into the seed
  if(left > 0){
    unsigned v = 0;
    memcpy(&v, str, left);
    seed = seed ^ v;
  }

  return seed;
}

unsigned getTimestamp()
{
#if __xcore__
  return get_reference_time();
#else
  return 0;
#endif
}

int32_t double_to_int32(double d, const int d_exp){
    int m_exp;
    double m = frexp (d, &m_exp);

    double r = ldexp(m, m_exp - d_exp);
    int output_exponent;
    frexp(r, &output_exponent);
    if(output_exponent>31){
        printf("exponent is too high to cast to an int32_t (%d)\n", output_exponent);
        assert(0);
    }
    return r;
}

unsigned vector_int32_maxdiff(int32_t * B, int B_exp, double * f, int start, int count){
    unsigned max_diff = 0;

    for(int i=start;i<start + count;i++){
        int32_t v = double_to_int32(f[i], B_exp);
        int diff = v-B[i];
        if (diff < 0 ) diff = -diff;
        if( (unsigned)diff > max_diff){
            max_diff = (unsigned)diff;
        }
    }
    return max_diff;
}

unsigned vector_int16_maxdiff(int16_t * B, int B_exp, double * f, int start, int count){
    unsigned max_diff = 0;

    for(int i=start;i<start + count;i++){
        //double_to_int32() is still the right conversion: the reference value is by construction
        //within the 16 bit range at this exponent, and the difference is reported in mantissa units
        //of that exponent either way.
        int32_t v = double_to_int32(f[i], B_exp);
        int diff = v-B[i];
        if (diff < 0 ) diff = -diff;
        if( (unsigned)diff > max_diff){
            max_diff = (unsigned)diff;
        }
    }
    return max_diff;
}
