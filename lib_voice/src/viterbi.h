#ifndef VITERBI_H
#define VITERBI_H

#include <stdint.h>
#include "states.h"

typedef struct viterbi {
    int sum[STATES];
    unsigned char memory[MAXSTEPS][STATES];
    int fail;
    int step;
} viterbi;


void viterbi_clear(viterbi *v);
int viterbi_integrate_vector(viterbi *v, hmm *model, int32_t vector[FEATURES+1]);
int viterbi_final(viterbi *v, hmm *model);

#endif
