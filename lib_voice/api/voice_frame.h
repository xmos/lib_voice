#ifndef VOICE_FRAME_H
#define VOICE_FRAME_H

#include "viterbi.h"

#define FRAME_INCREMENT  160
#define FRAME_LENGTH     512

typedef struct {
    short samples[FRAME_LENGTH];
    int noise[FEATURES+1];
    int index;
    int listening, final, initial;
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
#define FRAME_SPEAKING                1
#define FRAME_SPOKEN                  2

/** Function that performs feature extraction This function must be called
 * when frame_add_sample_is_full returns 1. It will compute the features
 * that can be passed to the model checker. This function returns one of
 * the defines above: FRAME_QUIET, FRAME_SPEAKING or FRAME_SPOKEN.
 * FRAME_QUIET means that there is nothing there. FRAME_SPEAKING means that
 * voice is being analysed, the model checker below will try and make sense
 * of it; FRAME_SPOKEN is returned exactly once for each word, the model
 * checker below will try and make sense of it and return the final verdict
 *
 * \param f         Frame to analyse
 * \param dctValues Feature vector
 */
int frame_feature_extract(voice_frame *f, int dctValues[FEATURES+1]);

/** Function that compares extracted features to a model
 *
 * This function should be called for each model after
 * frame_feature_extract. It returns either 1 or 0; 1 to indicate that the
 * model matches (thus far), 0 to indicate that it does not match (so far).
 * Only when frame_feature_extract returns FRAME_SPOKEN should this value
 * be treated as final; when frame_feature_extract returns FRAME_SPEAKING
 * this values can be ignored or treated as an early assessment.
 * 
 * \param f                Frame that has been analysed
 * \param dctValues        Feature vector
 * \param keyword_model    Model to compare against
 * \param keyword_progress structure that holds progress through the model
*/
int frame_model_matches(voice_frame *f, int dctValues[FEATURES+1], hmm *keyword_model, viterbi *kewyord_progress);

#endif
