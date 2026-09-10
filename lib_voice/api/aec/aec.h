// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef AEC_H
#define AEC_H

#include <stdio.h>
#include <string.h>
#include "xmath/xmath.h"
#include "aec_defines.h"
#include "aec_state.h"
#include "aec_schedule.h"
#include "aec_memory_pool.h"

/** Reference input level above which it is considered active
 *
 * @ingroup aec_types
 */
#define REF_ACTIVE_THRESHOLD_DB (-60)


/**
 * @defgroup aec_func     AEC API Functions
 */

/**
 * @brief Initialise the AEC for a given configuration
 *
 * This initializes the aggregated AEC state for the
 * supplied runtime configuration (number of channels and filter phases).
 * Call once at startup, and again whenever the AEC configuration changes.
 *
 * During initialisation, `aec_init()` binds the AEC internal BFP structures to
 * memory provided by the AEC memory pools. The amount of memory consumed depends
 * on the runtime configuration.
 *
 * @note
 * The caller must provide memory pools sized for the maximum compile-time AEC configuration
 * (@ref AEC_MAX_Y_CHANNELS, @ref AEC_MAX_X_CHANNELS, @ref AEC_MAIN_FILTER_PHASES, @ref AEC_SHADOW_FILTER_PHASES).
 * `aec_init()` assigns memory from these pools based on the
 * runtime configuration supplied. The memory pools must remain valid
 * for the lifetime of the AEC instance.
 * Any change to the number of channels or filter phases requires calling
 * `aec_init()` again to rebind internal state to the memory pools.
 * See @ref aec_memory_pool_t and @ref aec_shadow_filt_memory_pool_t for details
 * on memory pool sizing and usage.
 *
 * \par Preconditions
 * \anchor aec_phase_pool_capacity The runtime configuration must fit the compile-time one. All of
 * the following must hold:
 * - num_y_channels <= @ref AEC_MAX_Y_CHANNELS
 * - num_x_channels <= @ref AEC_MAX_X_CHANNELS
 * - num_x_channels * num_main_filter_phases <= @ref AEC_LIB_MAX_PHASES
 * - num_x_channels * num_shadow_filter_phases <= @ref AEC_LIB_MAX_PHASES
 * - AEC_MAIN_POOL_BYTES(num_y_channels, num_x_channels, num_main_filter_phases)
 *   <= sizeof(@ref aec_memory_pool_t)
 * - AEC_SHADOW_POOL_BYTES(num_y_channels, num_x_channels, num_shadow_filter_phases)
 *   <= sizeof(@ref aec_shadow_filt_memory_pool_t)
 *
 * The two @ref AEC_LIB_MAX_PHASES conditions are array bounds rather than memory pool capacity:
 * aec_filter_state_t::h_hat and aec_filter_state_t::X_fifo_1d address one y channel's phases across
 * all x channels with a single index bounded by @ref AEC_LIB_MAX_PHASES, so it is that index, not
 * the phase count of the whole filter, which has to fit. de_output_t::phase_power is bounded the
 * same way.
 *
 * The last two conditions are the memory pool capacity, and they are stated in bytes because they
 * cannot be expressed as a comparison of phase counts. Each pool is a single linear allocation
 * arena, and the phases drawn from it are not all the same size: a main or shadow filter phase is
 * @ref AEC_FRAME_ADVANCE `int32_t` (the filter is held in the time domain) while an X FIFO phase is
 * @ref AEC_FD_FRAME_LENGTH `complex_s32_t`, so exchanging one for the other changes the total. The
 * remaining allocations scale with the channel counts, and a configuration using fewer channels
 * than the build allows leaves room that phases can be allocated from. A rule counting only phases
 * is therefore neither necessary nor sufficient: it admits configurations that overrun the pool and
 * rejects configurations that fit comfortably.
 *
 * @ref AEC_MAIN_POOL_BYTES and @ref AEC_SHADOW_POOL_BYTES compute the exact demand and are compile
 * time constants for compile time arguments, so an application with a fixed runtime configuration
 * can check these conditions with `_Static_assert` - see the assertions on ADEC's delay estimation
 * configuration in `adec_defines.h` for an example. `aec_init()` also asserts all of the above, so
 * a configuration which breaks them fails at initialisation rather than corrupting memory beyond
 * the pool.
 *
 * @param[inout] aec_state                AEC state object
 * @param[in]    num_y_channels           Number of microphone input channels
 * @param[in]    num_x_channels           Number of reference input channels
 * @param[in]    num_main_filter_phases   Number of phases in the main filter
 * @param[in]    num_shadow_filter_phases Number of phases in the shadow filter
 * @param[in]    tdist                    Work distribution to use for scheduling AEC work
 *                                        across hardware threads in `aec_process_frame()`.
 *                                        Use a library default for 2ch, 1 or 2 threads,
 *                                        otherwise provide an application-defined schedule.
 *
 * @par Example
 * @code{.c}
 *   aec_state_t aec;
 *   unsigned y_chans = 2, x_chans = 2;
 *   unsigned main_phases = 10, shadow_phases = 5;
 *
 *   // Use a library-provided default schedule (2ch, 1 thread)
 *   aec_init(&aec, y_chans, x_chans, main_phases, shadow_phases, &aec_tdist_chans2_threads1);
 *
 *   // Or, passing a custom schedule generated via setting `AEC_SCHEDULE_CONFIG` in CMake
 *   // extern aec_task_distribution_t tdist;
 *   // aec_init(&aec, y_chans, x_chans, main_phases, shadow_phases, &tdist);
 * @endcode
 *
 * @ingroup aec_func
 */
void aec_init(
        aec_state_t *aec_state,
        unsigned num_y_channels,
        unsigned num_x_channels,
        unsigned num_main_filter_phases,
        unsigned num_shadow_filter_phases,
        const aec_task_distribution_t *tdist);

/**
 * @brief Process a frame of microphone samples using the AEC
 *
 * This function performs acoustic echo cancellation on a frame of input microphone
 * samples. It uses the input reference data frame to model the room echo characteristics
 * and adapt the internal main and shadow filters.
 *
 * @param[inout] aec_state     AEC state structure
 * @param[inout] output_main   Output from processing the mic input through the main filter
 * @param[inout] output_shadow Output from processing the mic input through the shadow filter
 * @param[in] y_data           Input microphone data frame
 * @param[in] x_data           Input reference data frame
 *
 * @ingroup aec_func
 */
void aec_process_frame(
        aec_state_t *aec_state,
        int32_t (*output_main)[AEC_FRAME_ADVANCE],
        int32_t (*output_shadow)[AEC_FRAME_ADVANCE],
        int32_t (*y_data)[AEC_FRAME_ADVANCE],
        int32_t (*x_data)[AEC_FRAME_ADVANCE]);

/** @brief Detect activity on input channels.
 *
 * This function implements a quick check for detecting activity on the input channels. It detects signal presence by checking
 * if the maximum sample in the time domain input frame is above a given threshold.
 *
 * @param[in] input_data Pointer to input data frame. Input is assumed to be in Q1.31 fixed point format.
 * @param[in] active_threshold Threshold for detecting signal activity
 * @param[in] num_channels Number of input data channels
 * @returns 0 if no signal activity on the input channels, 1 if activity detected on the input channels
 *
 * @ingroup aec_func
 */
uint32_t aec_detect_input_activity(const int32_t (*input_data)[AEC_FRAME_ADVANCE], float_s32_t active_threshold, int32_t num_channels);

/**
 * @brief Calculate energy in the spectrum
 *
 * This function calculates the energy of frequency domain data used in the AEC. Frequency domain data in AEC is in the form of complex 32bit vectors and energy is calculated as the squared magnitude of the input vector.
 *
 * @param[out] fd_energy energy of the input spectrum
 * @param[in] input input spectrum BFP structure
 *
 * @ingroup aec_func
 */
void aec_calc_freq_domain_energy(
        float_s32_t *fd_energy,
        const bfp_complex_s32_t *input);

/** @brief Reset parts of aec state structure.
 *
 * This function resets parts of AEC state so that the echo canceller starts adapting from a zero filter.
 *
 * @param[inout] aec_state     AEC state structure
 *
 * @ingroup aec_func
 */
void aec_reset_state(aec_state_t *aec_state);

/** @brief Calculate the energy of the input signal
 *
 * This function calculates the sum of the energy across all samples of the time domain input channel and
 * returns the maximum energy across all channels.
 *
 * @param[in] input_data Pointer to the input data buffer. The input is assumed to be in Q1.31 fixed point format.
 * @param[in] num_channels Number of input channels.
 * @returns Maximum energy in float_s32_t format.
 *
 * @ingroup aec_func
 *
 */
float_s32_t aec_calc_max_input_energy(
        const int32_t (*input_data)[AEC_FRAME_ADVANCE],
        int num_channels);

/** @brief Calculate a correlation metric between the microphone input and estimated microphone signal
 *
 * This function calculates a metric of resemblance between the mic input and the estimated mic signal. The correlation
 * metric, along with reference signal energy is used to infer presence of near and far end signals in the AEC mic
 * input.
 *
 * @param[in] state AEC state structure. `state->y` and `state->y_hat` are used to calculate the correlation metric
 * @param[in] ch mic channel index for which to calculate the metric
 * @returns correlation metric in float_s32_t format
 *
 * @ingroup aec_func
 *
 */
float_s32_t aec_calc_corr_factor(
        aec_filter_state_t *state,
        unsigned ch);

/** @brief Find where a time domain filter tap lives within a stored filter phase
 *
 * The taps of an aec_filter_state_t::h_hat phase are not stored in time order - they are permuted into the
 * bit-reversed index order the FFT works in, which lets the AEC skip the index bit-reversal pass on both of the
 * per-phase transforms it does every frame. This function maps a tap's position in the impulse response to its
 * position in the stored phase, so that code reading or writing the filter can account for the permutation:
 *
 * \code
 *      // read tap n of the impulse response of phase ph
 *      int32_t tap = state->h_hat[ch][ph].data[aec_h_hat_tap_index(n)];
 * \endcode
 *
 * The mapping is a permutation of `[0, AEC_FRAME_ADVANCE)` onto itself, so it can be used in either direction. It is
 * only needed by code that cares about the *order* of the taps; anything order independent (per-phase energy, copying
 * or zeroing a whole phase) can use the stored data directly.
 *
 * @param[in] n Position of the tap in the filter phase's impulse response, less than @ref AEC_FRAME_ADVANCE
 * @returns Index of that tap within the stored filter phase
 *
 * @ingroup aec_func
 *
 */
unsigned aec_h_hat_tap_index(unsigned n);

#endif
