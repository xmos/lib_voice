#include "viterbi.h"
#include <stdio.h>

int verbose = 1;

static int sq(int x) {
    return x*x;
}

static int log_gaussian(int av, int sdev, int val) {
    return sq(val - av)/(sdev*sdev);
}
    
static int probability_vector(int vector[FEATURES], int state, int step, viterbi *v, hmm *model) {
    int prod = 0;
    for(int i = 0; i < model->USED_FEATURES; i++) {
        int prob = log_gaussian(model->states[state].gauss[i].av,
                                model->states[state].gauss[i].sdev,
                                vector[i+1]);
        prod += prob;
    }
    int redProd = prod;
    if (redProd > 255) {
        redProd = 255;
    }
    v->memory[step][state] = redProd;
    return prod;
}

void viterbi_clear(viterbi *v) {
    for(int s = 0; s < STATES; s++) {
        v->sum[s] = 0;
    }
    v->fail = 0;
    v->step = 0;
}

int viterbi_integrate_vector(viterbi *v, hmm *model, int vector[FEATURES]) {
    v->step++;
    for(int s = STATES-1; s > 0; s--) {
        int p = probability_vector(vector, s, v->step, v, model);
        int v0 = v->sum[s-1] + p;
        int v1 = v->sum[s] + p;
        if (v0 < v1) {
            v->sum[s] = v0;
        } else {
            v->sum[s] = v1;
        }
    }
    v->sum[0] = v->sum[0] + probability_vector(vector, 0, v->step, v, model);
    int max = v->sum[0];
    int maxs = 0;
    for(int s = 1; s < STATES; s++) {
        if (v->sum[s] < max) {
            max = v->sum[s];
            maxs = s;
        }
    }
    if (max > model->EARLY_FAIL) {
        v->fail = 1;
    }
    if (v->fail) {
        return 0;
    }
    return v->sum[maxs]/v->step < model->SUCCESS_AVG;
}

static int prob_memory(viterbi *v, hmm *model, int step, int state) {
    int prod = v->memory[step][state];
    int start = (model->states[state].expected_start_frac * (v->step+1)) >> 8;
    int end = (model->states[state].expected_end_frac * (v->step+1)) >> 8;
    int early_steps = start - step;
    if (early_steps > 0) {
        prod += 3 * model->USED_FEATURES * early_steps / 2;
    }
    int late_steps = step - end;
    if (late_steps > 0) {
        prod += 3 * model->USED_FEATURES * late_steps / 2;
    }
    if (verbose) printf(" (%d,%d) %2d %2d", start, end, early_steps, late_steps);
    return prod;
}

int viterbi_final(viterbi *v, hmm *model) {
    if (v->step > 40 && v->step < 85) {
    int omax = 0;
        for(int i = 0; i < STATES; i++) {
            v->sum[i] = 0;
        }
        for(int step = 1; step <= v->step; step++) {
            for(int s = STATES-1; s > 0; s--) {
                int p = prob_memory(v, model, step, s);
                int v0 = v->sum[s-1] + p;
                int v1 = v->sum[s] + p;
                if (v0 < v1) {
                    v->sum[s] = v0;
                } else {
                    v->sum[s] = v1;
                }
           }
            v->sum[0] = v->sum[0] + prob_memory(v, model, step, 0);
            int max = v->sum[0];
            int maxs = 0;
            for(int s = 1; s < STATES; s++) {
                if (v->sum[s] < max) {
                    max = v->sum[s];
                    maxs = s;
                }
            }
            if (verbose) {
                printf(" ");
                for(int s = 0; s < STATES; s++) {
                    printf("%9d%c ", v->sum[s], maxs == s ? '*':' ');
                }
                printf("   max so far %2d %5d diff %5d\n", step,  v->sum[maxs]/step, max - omax);
                omax = max;
            }
        }
        if (verbose) {
            printf("%6d (%d) %s\n", v->sum[STATES-1]/v->step, v->step, v->fail ? "Failed":"");
        }
        if (v->fail) {
            return 0;
        }
        return v->sum[STATES-1]/v->step < model->SUCCESS_AVG;
    }
    return 0;
}
