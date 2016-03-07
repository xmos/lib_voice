#ifndef VOICE_FRAME_H
#define VOICE_FRAME_H

#include "viterbi.h"

#define FRAME_INCREMENT  160
#define FRAME_LENGTH     512

typedef struct {
    viterbi v;
    short samples[FRAME_LENGTH];
    int noise[FEATURES+1];
    int index;
    int listening;
    int matched;
} voice_frame;

/** Function that initialises a voice frame. Should be called on a declared
 * voice_frame
 */
void frame_initialise(voice_frame *f);

/** Function that adds a sample to a frame. It returns 1 if the frame is
 * now full and need analysing, or 0 otherwise.
 *
 * \param f      Frame to add the sample to
 * \param sample Sample value to add to the frame
 * \returns true if the frame is full and needs analysing
 */
int frame_add_sample_is_full(voice_frame *f, int sample);

#define FRAME_QUIET                   0
#define FRAME_SPEAKING_MATCHING       1
#define FRAME_SPEAKING_NOT_MATCHING   2
#define FRAME_SPOKEN_MATCHED          3
#define FRAME_SPOKEN_NOT_MATCHED      4

/** Function that analyses a frame.
 * This function must be called when frame_add_sample_is_full returns 1. 
 * It will analyse the added speech, and return one of the defines above.
 * It will also create space in the frame to add more data.
 * This function takes no more than XXXX thread cycles to complete.
 *
 * \param f      Frame to analyse
 */
int frame_analyse(voice_frame *f);

#endif
