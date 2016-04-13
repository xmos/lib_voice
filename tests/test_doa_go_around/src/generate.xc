#include <stdio.h>
#include "lib_dsp_math.h"
#include "lib_voice_doa_naive.h"

static int vector;
static int value;
static int max = 0x60000000;

int abs(int a) {
    return a > 0 ? a : -a;
}


int sign(int a) {
    return a < 0 ? -1 : 1;
}

unsigned seed;

unsigned random() {
    seed = seed * 1234567 + 1;
    return seed;
}

unsigned amplitude = 0x02000000;

int nextSample() {
    value = value * 9 / 10 + vector;
    vector = vector * 7 / 10 + random() % amplitude - amplitude/2;
    if (abs(value) > max && sign(vector) == sign(value)) {
        vector = -vector;
    }
    return value;
}

#define BUFF 128
int mic[6][BUFF];

#define D  43 // mm
#define D3 37 // mm (0.5 sqrt(3) D)

int xlocs[6] = {D/2, D, D/2, -D/2, -D, -D/2};
int ylocs[6] = {D3, 0, -D3, -D3, 0, D3};
    
int main() {
    struct lib_voice_doa d;
    lib_voice_doa_naive_init(d);
    int time = 0;
    for(int i = 0; i < 32000; i+= 16000*30/360) {
        int degrees;
        for(int j = 0; j < 256; j++) {
            int rad = i * (PI2_Q8_24 / 16000);
            int x = ((lib_dsp_math_sin(rad) >> 8) * 1000) >> 16;
            int y = ((lib_dsp_math_cos(rad)>> 8) * 1000) >> 16;
            int sample = 8*nextSample();
            for(int m = 0; m < 6; m++) {
                int xd = x - xlocs[m];
                int yd = y - ylocs[m];
                int sum = xd * xd + yd * yd;
                int md = (lib_dsp_math_squareroot(sum<<10) >> 17)-40;
                md = md * 16000 / 343000;
                mic[m][(time + md)%BUFF] = sample;
                mic[m][(time + md + 1)%BUFF] = sample;
            }
            time = (time + 1) % BUFF;
            degrees = lib_voice_doa_naive_incorporate(d,
                                                          mic[0][time],
                                                          mic[1][time],
                                                          mic[2][time],
                                                          mic[3][time],
                                                          mic[4][time],
                                                      mic[5][time]);
        }
        printf("%5d %3d %d\n", i, i * 360 / 16000, degrees);
    }
    return 0;
}
