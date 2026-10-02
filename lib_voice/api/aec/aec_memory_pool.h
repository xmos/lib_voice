// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef AEC_MEMORY_POOL_H
#define AEC_MEMORY_POOL_H

#include "xmath/xmath.h"
#include "aec_defines.h"

/**
 * @defgroup aec_memory_pool AEC memory pool
 */

/**
 * @brief Round a pool allocation of `bytes` bytes up to a whole number of double words.
 *
 * `aec_init()` rounds every buffer it takes from @ref aec_memory_pool_t up to this size, so every
 * buffer starts on a double word boundary whatever the runtime configuration.
 *
 * @ingroup aec_memory_pool
 */
#define AEC_POOL_ALIGN(bytes) (((bytes) + 7) & ~(size_t)7)

/**
 * @brief Number of `type` elements an `n` element pool buffer takes up once rounded up by
 * @ref AEC_POOL_ALIGN.
 *
 * @ingroup aec_memory_pool
 */
#define AEC_POOL_LEN(type, n) (AEC_POOL_ALIGN((n) * sizeof(type)) / sizeof(type))

/**
 * @brief aec_memory_pool_t
 *
 * Memory pool for the AEC main filter, shadow filter and shared state buffers.
 *
 * This pool provides contiguous storage for all BFP (block floating-point from ``lib_xcore_math``) structures used by
 * the main and shadow filters (`aec_filter_state_t`) and the shared filter state (`aec_shared_filter_state_t`).
 * The `aec_init()` function initializes the BFP structures in the AEC state structures to point to
 * memory buffers from this pool during AEC initialisation.
 *
 * The memory pool allocates storage based on AEC compile-time configuration parameters:
 * - @ref AEC_MAX_Y_CHANNELS
 * - @ref AEC_MAX_X_CHANNELS
 * - @ref AEC_MAIN_FILTER_PHASES
 * - @ref AEC_SHADOW_FILTER_PHASES
 *
 * The same pool can be used to initialize AEC for any runtime configuration (passed as arguments to `aec_init()`)
 * which satisfies @ref aec_phase_pool_capacity. `aec_init()` allocates the main filter and shared state from the
 * start of the pool and the shadow filter straight after them, so a runtime configuration with a shorter shadow
 * filter than the compile time one can use the spare memory for a longer main filter. The ADEC delay estimation
 * configuration relies on this, as it has no shadow filter.
 *
 * @note
 * This structure exists to own memory, not to describe layout.
 * The memory pool acts as a linear allocation arena used by `aec_init()` to
 * initialise BFP structures in the AEC filter state structures. Memory is assigned
 * sequentially from the pool based on the runtime configuration (number of
 * channels and filter phases), and does not have a fixed or semantic mapping to
 * the individual fields of this struct.
 * The named members of this struct exist only to reserve sufficient contiguous
 * storage at compile time. They must not be interpreted as backing specific
 * components of `aec_filter_state_t`/`aec_shared_filter_state_t` and should never be accessed directly. After
 * initialisation, all access to this memory occurs exclusively through the BFP
 * structures owned by the AEC state.
 *
 * Every buffer `aec_init()` allocates is rounded up to a whole number of double words
 * (@ref AEC_POOL_ALIGN), so each one starts on a double word boundary and can be accessed with
 * double word loads and stores. Each row of the members below is rounded up the same way
 * (@ref AEC_POOL_LEN) so that the pool reserves that padding too.
 *
 * @ingroup aec_memory_pool
 */
typedef struct {
    /** Memory pointed to by aec_shared_filter_state_t::y and aec_shared_filter_state_t::Y*/
    int32_t DWORD_ALIGNED mic_input_frame[AEC_MAX_Y_CHANNELS]
                                         [AEC_POOL_LEN(int32_t, AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING)];
    /** Memory pointed to by aec_shared_filter_state_t::x and aec_shared_filter_state_t::X. Also reused for main filter
     * aec_filter_state_t::T*/
    int32_t DWORD_ALIGNED ref_input_frame[AEC_MAX_X_CHANNELS]
                                         [AEC_POOL_LEN(int32_t, AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING)];
    /** Memory pointed to by aec_shared_filter_state_t::prev_y*/
    int32_t DWORD_ALIGNED mic_prev_samples[AEC_MAX_Y_CHANNELS]
                                          [AEC_POOL_LEN(int32_t, AEC_PROC_FRAME_LENGTH - AEC_FRAME_ADVANCE)];
    /** Memory pointed to by aec_shared_filter_state_t::prev_x*/
    int32_t DWORD_ALIGNED ref_prev_samples[AEC_MAX_X_CHANNELS]
                                          [AEC_POOL_LEN(int32_t, AEC_PROC_FRAME_LENGTH - AEC_FRAME_ADVANCE)];
    /** Memory pointed to by main and shadow filter aec_filter_state_t::h_hat. The filters are stored in the time domain
     * as AEC_FRAME_ADVANCE length real 16bit phases.*/
    int16_t DWORD_ALIGNED phase_pool_h_hat[AEC_MAX_Y_CHANNELS * AEC_MAX_X_CHANNELS
                                           * (AEC_MAIN_FILTER_PHASES + AEC_SHADOW_FILTER_PHASES)]
                                          [AEC_POOL_LEN(int16_t, AEC_FRAME_ADVANCE)];
    /** Memory pointed to by aec_shared_filter_state_t::X_fifo, main filter aec_filter_state_t::X_fifo_1d and shadow
     * filter aec_filter_state_t::X_fifo_1d*/
    complex_s32_t DWORD_ALIGNED phase_pool_X_fifo[AEC_MAX_X_CHANNELS * AEC_MAIN_FILTER_PHASES]
                                                 [AEC_POOL_LEN(complex_s32_t, AEC_FD_FRAME_LENGTH)];
    /** Memory pointed to by main and shadow filter aec_filter_state_t::Error and aec_filter_state_t::error*/
    complex_s32_t DWORD_ALIGNED Error[2 * AEC_MAX_Y_CHANNELS][AEC_POOL_LEN(complex_s32_t, AEC_FD_FRAME_LENGTH)];
    /** Memory pointed to by main and shadow filter aec_filter_state_t::Y_hat and aec_filter_state_t::y_hat*/
    complex_s32_t DWORD_ALIGNED Y_hat[2 * AEC_MAX_Y_CHANNELS][AEC_POOL_LEN(complex_s32_t, AEC_FD_FRAME_LENGTH)];
    /** Memory pointed to by shadow filter aec_filter_state_t::T*/
    complex_s32_t DWORD_ALIGNED T[AEC_MAX_X_CHANNELS][AEC_POOL_LEN(complex_s32_t, AEC_FD_FRAME_LENGTH)];
    /** Memory pointed to by main and shadow filter aec_filter_state_t::X_energy*/
    int32_t DWORD_ALIGNED X_energy[2 * AEC_MAX_X_CHANNELS][AEC_POOL_LEN(int32_t, AEC_FD_FRAME_LENGTH)];
    /** Memory pointed to by aec_shared_filter_state_t::sigma_XX*/
    int32_t DWORD_ALIGNED sigma_XX[AEC_MAX_X_CHANNELS][AEC_POOL_LEN(int32_t, AEC_FD_FRAME_LENGTH)];
    /** Memory pointed to by main and shadow filter aec_filter_state_t::inv_X_energy*/
    int32_t DWORD_ALIGNED inv_X_energy[2 * AEC_MAX_X_CHANNELS][AEC_POOL_LEN(int32_t, AEC_FD_FRAME_LENGTH)];
    /** Memory pointed to by main and shadow filter aec_filter_state_t::overlap*/
    int32_t DWORD_ALIGNED overlap[2 * AEC_MAX_Y_CHANNELS][AEC_POOL_LEN(int32_t, AEC_FRAME_OVERLAP)];
}aec_memory_pool_t;

/**
 * @brief Bytes `aec_init()` reserves in @ref aec_memory_pool_t for a runtime configuration.
 *
 * This includes rounding every buffer up to a whole number of double words (@ref AEC_POOL_ALIGN).
 * It is a compile time constant for compile time arguments, so it can be used in a
 * `_Static_assert` to check a fixed runtime configuration against the pool.
 *
 * AEC_POOL_BYTES(num_y, num_x, num_main_phases, num_shadow_phases) must always be
 * <= sizeof(aec_memory_pool_t)
 *
 * @ingroup aec_memory_pool
 */
#define AEC_POOL_BYTES(num_y, num_x, num_main_phases, num_shadow_phases) ( \
      ((num_y) + (num_x)) * AEC_POOL_ALIGN((AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING) * sizeof(int32_t)) \
    + ((num_y) + (num_x)) * AEC_POOL_ALIGN((AEC_PROC_FRAME_LENGTH - AEC_FRAME_ADVANCE) * sizeof(int32_t)) \
    + (num_y) * (num_x) * ((num_main_phases) + (num_shadow_phases)) * AEC_POOL_ALIGN(AEC_FRAME_ADVANCE * sizeof(int16_t)) \
    + (num_x) * (num_main_phases) * AEC_POOL_ALIGN(AEC_FD_FRAME_LENGTH * sizeof(complex_s32_t)) \
    + (4 * (num_y) + (num_x)) * AEC_POOL_ALIGN(AEC_FD_FRAME_LENGTH * sizeof(complex_s32_t)) \
    + 5 * (num_x) * AEC_POOL_ALIGN(AEC_FD_FRAME_LENGTH * sizeof(int32_t)) \
    + 2 * (num_y) * AEC_POOL_ALIGN(AEC_FRAME_OVERLAP * sizeof(int32_t)) )

/* Assert that the compile-time pool size matches the calculated byte requirement for the maximum
configuration */
_Static_assert(AEC_POOL_BYTES(AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS, AEC_MAIN_FILTER_PHASES,
                              AEC_SHADOW_FILTER_PHASES) == sizeof(aec_memory_pool_t),
        "AEC_POOL_BYTES() no longer matches aec_memory_pool_t - update it to match the allocations "
        "made by aec_priv_main_init() and aec_priv_shadow_init()");
#endif
