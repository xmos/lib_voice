#ifndef STATES_H
#define STATES_H

#define STATES 4
#define FEATURES 23
#define MAXSTEPS 92

typedef struct {
    unsigned char expected_start_frac, expected_end_frac;
    struct {
        int av, sdev;
    } gauss[FEATURES];
} state;

typedef struct {
    int SUCCESS_AVG;
    int EARLY_FAIL;
    int USED_FEATURES;
    state states[STATES];
} hmm;

extern hmm suzy_mark;
extern hmm suzy_al_ro;

#endif
