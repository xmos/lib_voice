#ifndef VITERBI_H
#define VITERBI_H

#include "states.h"

typedef struct viterbi {
    int sum[STATES];
    unsigned char memory[MAXSTEPS][STATES];
    int fail;
    int step;
} viterbi;


void viterbi_clear(viterbi *v);
int viterbi_integrate_vector(viterbi *v, hmm *model, int vector[FEATURES]);
int viterbi_final(viterbi *v, hmm *model);

#endif
