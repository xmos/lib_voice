#ifndef STATES_H
#define STATES_H

#ifndef KEYWORD_SUZY
#define KEYWORD_SUZY 0
#endif

#ifndef KEYWORD_ALEXA
#define KEYWORD_ALEXA 1
#endif

#if KEYWORD_SUZY && KEYWORD_ALEXA
#error "Cannot enable multiple keywords"
#endif

#if !KEYWORD_SUZY && !KEYWORD_ALEXA
#error "No keyword enabled"
#endif

#if KEYWORD_SUZY
#define STATES 4
#elif KEYWORD_ALEXA
#define STATES 6
#else
#error "Cannot define correct number of STATES"
#endif
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

#ifndef SPEAKER_HM
#define SPEAKER_HM 1
#endif

#ifndef SPEAKER_ML
#define SPEAKER_ML 1
#endif

#ifndef SPEAKER_RO
#define SPEAKER_RO 1
#endif

#ifndef SPEAKER_SC
#define SPEAKER_SC 1
#endif

extern hmm suzy_mark;
extern hmm suzy_al_ro;
extern hmm suzy_post_illusonic_ro;

extern hmm alexa_hm;
extern hmm alexa_ml;
extern hmm alexa_ro;
extern hmm alexa_sc;

#endif
