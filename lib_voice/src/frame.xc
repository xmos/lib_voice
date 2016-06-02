#include <stdio.h>
#include <string.h>
#include "viterbi.h"
#include "kaiser.h"
#include "voice_frame.h"
#include "lib_dsp_dct.h"
#include "lib_dsp_fft.h"
#include "log_int.h"
#include "mel.h"

void frame_initialise(voice_frame *f) {
    memset(f->samples, 0, FRAME_LENGTH * sizeof(short));
    memset(f->noise, 0, (FEATURES+1) * sizeof(int));
    f->index = FRAME_LENGTH - FRAME_INCREMENT;
    f->listening = 0;
    f->final = 0;
    f->initial = 0;
}

int frame_add_sample_is_full(voice_frame *f, int sample) {
    f->samples[f->index++] = sample;
    return f->index == FRAME_LENGTH;
}

static inline int mul_mel(int x, int y) {
    long long z = x*(long long) y;
    return z >> MEL_SHIFT;
}

static void melCompute(int32_t melValues[FEATURES+2], lib_dsp_fft_complex_t pts[FRAME_LENGTH]) {
    int sumEven = 0, sumOdd = 0;
    int mels = 0;

    for(int i = 0; i < FEATURES+2; i++) {
        melValues[i] = 0;
    }
    for(int i = melStart; i < FRAME_LENGTH/2; i++) {
        sumEven = sumEven + mul_mel(melTable[i], pts[i].re);
        sumOdd = sumOdd + mul_mel(MEL_MAX - melTable[i], pts[i].re);
        if (melTable[i] == 0) {
            melValues[mels++] = sumEven;
            sumEven = 0;
        } else if (melTable[i] == MEL_MAX) {
            melValues[mels++] = sumOdd;
            sumOdd = 0;
        }
    }

    for(int i = 1; i < mels; i++) {
        unsigned int x = melValues[i];// - noise[i-1];
        if (x < 10) x = 10;
        melValues[i-1] = log_int(x);
    }
}


static inline int mul_31(int a, int b) {
    return (a * (long long) b) >> 31;
}

static void window_kaiser_short(lib_dsp_fft_complex_t pts[FRAME_LENGTH], short data[], const int hann[]) {
    for(int i = 0; i < (FRAME_LENGTH>>1); i++) {
        int s = hann[i];
        pts[i].re =                mul_31(data[               i], s);
        pts[i].im =                0;
        pts[FRAME_LENGTH-1-i].re = mul_31(data[FRAME_LENGTH-1-i], s);
        pts[FRAME_LENGTH-1-i].im = 0;
    }
}

int printDCTValues = 0;
int printMELValues = 0;

#define LOUDNESS_GONE_QUIESCENT 40000
#define LOUDNESS_GONE_NOISY     45000

int frame_feature_extract(voice_frame *f, int32_t dctValues[FEATURES+1]) {
    lib_dsp_fft_complex_t pts[FRAME_LENGTH];
    int32_t melValues[FEATURES+20];

    window_kaiser_short(pts, f->samples, kaiser_half_90_512);
    lib_dsp_fft_bit_reverse(pts, FRAME_LENGTH);
    lib_dsp_fft_forward(pts, FRAME_LENGTH, lib_dsp_sine_512);

    for(int i = 0; i < FRAME_LENGTH/2; i++) {
        pts[i].re = pts[i].re * pts[i].re + pts[i].im * pts[i].im;
    }
    
    melCompute(melValues, pts);
    int totalMel = 0;
    int totalNoise = 0;
    for(int i = 0; i <= FEATURES; i++) {
        totalMel += melValues[i];
    }
    if (totalMel < LOUDNESS_GONE_NOISY) {
        for(int i = 0; i <= FEATURES; i++) {
            f->noise[i] = ((f->noise[i] * 255)>>8) + melValues[i];
        }
    } else {
        for(int i = 0; i <= FEATURES; i++) {
            int noiseLevel = f->noise[i] >> 8;
            if (melValues[i] >= noiseLevel) {
                melValues[i] -= noiseLevel;
                totalNoise += noiseLevel;
            } else {
                totalNoise += melValues[i];
                melValues[i] = 0;
            }
        }        
    }
    if (printMELValues) {
        printf("MEL ");
        for(int i = 0; i <= FEATURES; i++) {
            printf("%4d ", melValues[i]);
        }
        printf("\n");
    }
    lib_dsp_dct_forward24(dctValues, melValues);
    if (printDCTValues) {
        printf("DCT ");
        for(int i = 0; i < FEATURES-1; i++) {
            printf("%6d ", dctValues[i]);
        }
        printf("\n");
    }
    
    for(int i = 0; i < FRAME_LENGTH - FRAME_INCREMENT; i++) {
        f->samples[i] = f->samples[i+FRAME_INCREMENT];
    }
    f->index -= FRAME_INCREMENT;
    
    int loudness = dctValues[0] + totalNoise;
    if (f->listening) {
        f->initial = 0;
        if (loudness < LOUDNESS_GONE_QUIESCENT) {
            f->listening = 0;
            f->final = 1;
            return FRAME_SPOKEN;
        }
        return FRAME_SPEAKING;
    } else {
        f->final = 0;
        if (loudness > LOUDNESS_GONE_NOISY) {
            f->initial = 1;
            f->listening = 1;
            return FRAME_SPEAKING;
        }
        return FRAME_QUIET;
    }
    
}

int frame_model_matches(voice_frame *f, int32_t dctValues[FEATURES+1], hmm *keyword_model, viterbi *keyword_progress) {
    int matched = 0;
    if (f->listening) {
        if (f->initial) {
            viterbi_clear(keyword_progress);
        }
        matched = viterbi_integrate_vector(keyword_progress, keyword_model, dctValues);
    } else if (f->final) {
        matched = viterbi_final(keyword_progress, keyword_model);
    }
    return matched;
}
