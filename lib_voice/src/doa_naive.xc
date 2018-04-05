// Copyright (c) 2016-2018, XMOS Ltd, All rights reserved
#include "lib_voice_doa_naive.h"
#include <xs1.h>
#include <xclib.h>
#include <stdio.h>

static void init_correlation(struct lib_voice_doa_correlation &c) {
    c.n = 0;
    c.x = 0;
    c.y = 0;
    c.x2 = 0;
    c.y2 = 0;
    c.xy = 0;
}

static void incorporate_correlation(int x, int y, struct lib_voice_doa_correlation &c) {
    x >>= 8;
    y >>= 8;
    c.n++;
    c.x += x;
    c.y += y;
    c.x2 += x * (long long) x;
    c.y2 += y * (long long) y;
    c.xy += x * (long long) y;
}

#if 0
static void finish_correlation(struct lib_voice_doa_correlation &c, int angle, int support[12], int rear_steer) {
    long long res = c.n * c.xy - c.x * c.y;
    long long d0 = c.n * c.x2 - c.x * c.x;
    long long d1 = c.n * c.y2 - c.y * c.y;
    res = res >> 28;
    res = res < 0 ? res * -res : res * res;
    d0 = d0 >> 32;
    d1 = d1 >> 32;
    d0 = (d0 * d1);
    if (d0 != 0) {
        res = res / d0;
    } else {
        res = 0;
    }
#ifdef PRINT_CORR
    printf("%7lld %2d  ", res, angle);
#endif
#else

#define  ldivu(D,R,H,L,N) asm("ldivu %0,%1,%2,%3,%4" : "=r" (D), "=r" (R): "r" (H), "r" (L), "r" (N))

static void finish_correlation(struct lib_voice_doa_correlation &c, int angle, int support[12], int rear_steer) {
   long long corr = c.n * c.xy - c.x * c.y;
   long long d0 = c.n * c.x2 - c.x * c.x;
   long long d1 = c.n * c.y2 - c.y * c.y;
   unsigned int d0_h = d0 >> 32;
   unsigned int d1_h = d1 >> 32;
   unsigned long long dd = d0_h * (long long) d1_h;
   corr = corr >> 28;
   unsigned long long scorr = corr*corr;
   int res;
   unsigned zeroes;
   asm("clz %0, %1" : "=r" (zeroes) : "r" ((unsigned) (dd >> 32)) );
   int shift = 32 - zeroes;
   dd >>= shift;
   scorr >>= shift;
   if (dd != 0) {
       int modulo_tmp;
       ldivu(res, modulo_tmp, (unsigned) (scorr >> 32), (unsigned) (scorr & 0xFFFFFFFFULL), (unsigned) (dd & 0xFFFFFFFFULL));
       if (corr < 0) {
           res = -res;
       } else {
           res = res;
       }
   } else {
       res = 0;
   }
#ifdef PRINT_CORR
   printf("%7lld %2d  ", res, angle);
#endif

#endif
    c.lt = (c.lt + 127 * res) >> 7;
    if (rear_steer) {
        support[angle] += res;
        angle ++;
        if (angle >= 12) {
            angle -= 12;
        }
        support[angle] += res >> 1;
        angle -= 2;
        if (angle < 0) {
            angle += 12;
        }
        support[angle] += res >> 1;
    } else {
        support[angle] += res;
        angle += 6;
        if (angle >= 12) {
            angle -= 12;
        }
        support[angle] += res;
    }
#ifdef PRINT_CORR
    for(int i = 0; i < 12; i++) {
        printf("%6d ", support[i]);
    }
    printf("\n");
#endif
    init_correlation(c);
}

void lib_voice_doa_naive_init(struct lib_voice_doa &d) {
    init_correlation(d.c62a);
    init_correlation(d.c63a);
    init_correlation(d.c13a);
    init_correlation(d.c14a);
    init_correlation(d.c24a);
    init_correlation(d.c25a);
    init_correlation(d.c63);
    init_correlation(d.c36);
    init_correlation(d.c14);
    init_correlation(d.c41);
    init_correlation(d.c25);
    init_correlation(d.c52);
    for(int i = 0; i < 12; i++) {
        d.ltsupport[i] = 0;
    }
    d.ooooch6=d.oooch6=d.ooch6=d.och6=0; 
    d.ooooch5=d.oooch5=d.ooch5=d.och5=0;
    d.ooooch4=d.oooch4=d.ooch4=d.och4=0;
    d.ooooch3=d.oooch3=d.ooch3=d.och3=0;
    d.ooooch2=d.oooch2=d.ooch2=d.och2=0;
    d.ooooch1=d.oooch1=d.ooch1=d.och1=0;
    d.omaxi = LIB_VOICE_DOA_NOTHING;
}

int lib_voice_doa_naive_incorporate(struct lib_voice_doa &d,
                                    int ch1, int ch2, int ch3,
                                    int ch4, int ch5, int ch6) {
    int retval = -1;
    incorporate_correlation(ch6, ch2,     d.c62a);
    incorporate_correlation(ch6, ch3,     d.c63a);
    incorporate_correlation(ch1, ch3,     d.c13a);
    incorporate_correlation(ch1, ch4,     d.c14a);
    incorporate_correlation(ch2, ch4,     d.c24a);
    incorporate_correlation(ch2, ch5,     d.c25a);
    incorporate_correlation(ch6, d.oooch3, d.c63);
    incorporate_correlation(ch1, d.oooch4, d.c14);
    incorporate_correlation(ch2, d.oooch5, d.c25);
    incorporate_correlation(d.oooch6, ch3, d.c36);
    incorporate_correlation(d.oooch1, ch4, d.c41);
    incorporate_correlation(d.oooch2, ch5, d.c52);
    if (d.c52.n == 256) {
        unsigned long long ox2 = d.c52.x2;
        int support[12];
        for(int i = 0; i < 12; i++) {
            support[i] = 0;
        }
        finish_correlation(d.c62a, 5, support, 0);
        finish_correlation(d.c63a, 4, support, 0);
        finish_correlation(d.c13a, 3, support, 0);
        finish_correlation(d.c14a, 2, support, 0);
        finish_correlation(d.c24a, 1, support, 0);
        finish_correlation(d.c25a, 0, support, 0);
        finish_correlation(d.c63, 7, support, 1);
        finish_correlation(d.c14, 5, support, 1);
        finish_correlation(d.c25, 3, support, 1);
        finish_correlation(d.c36, 1, support, 1);
        finish_correlation(d.c41,11, support, 1);
        finish_correlation(d.c52, 9, support, 1);
        int level = 32 - clz(ox2 >> 32);
        for(int i = 0; i < 12; i++) {
            d.ltsupport[i] = (d.ltsupport[i]*15 + support[i] * level) >> 4;
        }
#ifdef PRINT_CORR
        printf("******* **  ");
        for(int i = 0; i < 12; i++) {
            printf("%6d ", d.ltsupport[i]);
        }
        printf("\n");
#endif
        int sum = d.ltsupport[0];
        int max = d.ltsupport[0];
        int min = d.ltsupport[0];
        int maxi = 0;
        for(int i = 1; i < 12; i++) {
            sum += d.ltsupport[i];
            if (max < d.ltsupport[i]) {
                max = d.ltsupport[i];
                maxi = i;
            }
            if (min > d.ltsupport[i]) {
                min = d.ltsupport[i];
            }
        }

#ifndef DOA_NAIVE_DONT_THRESH
        int usable = level > 0 && max > 100;
#ifndef DONT_USE_MIN
        usable = usable && min > 3*max/4;
#endif
#else
        int usable = 1;
#endif

#ifdef PRINT_CORR
        printf("Level %d min %d max %d usable %d\n", level, min, max, usable);
#endif
        if (usable) {
            d.omaxi = maxi * 30;
        } else {
            d.omaxi = LIB_VOICE_DOA_NOTHING;
        }
    }
    retval = d.omaxi;
    d.ooooch6=d.oooch6; d.oooch6=d.ooch6; d.ooch6=d.och6; d.och6=ch6; 
    d.ooooch5=d.oooch5; d.oooch5=d.ooch5; d.ooch5=d.och5; d.och5=ch5; 
    d.ooooch4=d.oooch4; d.oooch4=d.ooch4; d.ooch4=d.och4; d.och4=ch4; 
    d.ooooch3=d.oooch3; d.oooch3=d.ooch3; d.ooch3=d.och3; d.och3=ch3; 
    d.ooooch2=d.oooch2; d.oooch2=d.ooch2; d.ooch2=d.och2; d.och2=ch2; 
    d.ooooch1=d.oooch1; d.oooch1=d.ooch1; d.ooch1=d.och1; d.och1=ch1;
    return retval;
}
