// Copyright (c) 2016-2018, XMOS Ltd, All rights reserved
#ifndef LIB_VOICE_DOA_NAIVE_H
#define LIB_VOICE_DOA_NAIVE_H

enum doaResId {
    DOA_RESID = 0,
    DOA_RESID_COUNT
};

enum doaCmd
{
    DOA_CMD_EN = 0,
    DOA_CMD_DIR, 
    DOA_CMD_COUNT
};

#define LIB_VOICE_DOA_NOTHING -1

struct lib_voice_doa_correlation {
    int n;
    long long x, x2;
    long long y, y2;
    long long xy;
    int lt;
};

/* This needs some refactoring
 */
typedef struct lib_voice_doa {
    struct lib_voice_doa_correlation c62a, c63a, c13a, c14a, c24a, c25a;
    struct lib_voice_doa_correlation c36, c63, c41, c14, c52, c25;
    int och6, och1, och2, och3, och4, och5;
    int ooch6, ooch1, ooch2, ooch3, ooch4, ooch5;
    int oooch6, oooch1, oooch2, oooch3, oooch4, oooch5;
    int ooooch6, ooooch1, ooooch2, ooooch3, ooooch4, ooooch5;
    int omaxi;
    int ltsupport[12];
} lib_voice_doa_t;

/** Function that initialises the DOA mechanism. Call once, with a DOA structure
 *
 * \param d  the structure that holds all DOA information
 */
void lib_voice_doa_naive_init(struct lib_voice_doa &d);

/** Function that incorporates a 16 kHz frame into the DOA information. It
 * occasionally returns an angle estimating the sound direction. There is
 * no filtering on this angle so it may change abruptly. Any filtering has
 * to be performed by the callee based on a model as to how fast the
 * direction may change.
 *
 * \param d    the structure that holds all DOA information
 * \param ch1  sample value on microphone 1
 * \param ch2  sample value on microphone 2
 * \param ch3  sample value on microphone 3
 * \param ch4  sample value on microphone 4
 * \param ch5  sample value on microphone 5
 * \param ch6  sample value on microphone 6
 * \returns a value that is either:
 *            LIB_VOICE_DOA_NOTHING: there is no sound to base DOA on.
 *            0..359:       the direction of the signal.
 *                          0 is from MIC0 to USB, then
 *                          counterclockwise in degrees.
 */
int lib_voice_doa_naive_incorporate(struct lib_voice_doa &d,
                          int ch1, int ch2, int ch3,
                          int ch4, int ch5, int ch6);

#endif
