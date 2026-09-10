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
 * @brief aec_memory_pool_t
 *
 * Memory pool for AEC main filter and shared state buffers.
 *
 * This pool provides contiguous storage for all BFP (block floating-point from ``lib_xcore_math``) structures used by the main filter (`aec_filter_state_t`)
 * and the shared filter state (`aec_shared_filter_state_t`).
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
 * which satisfies @ref aec_phase_pool_capacity. Note that this is not simply a matter of the runtime phase counts
 * being smaller than the compile-time ones: the phases drawn from a pool are not all the same size, so what has to
 * fit is the byte demand, which @ref AEC_MAIN_POOL_BYTES and @ref AEC_SHADOW_POOL_BYTES compute.
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
 * @ingroup aec_memory_pool
 */
typedef struct {
    /** Memory pointed to by aec_shared_filter_state_t::y and aec_shared_filter_state_t::Y*/
    int32_t mic_input_frame[AEC_MAX_Y_CHANNELS][AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING];
    /** Memory pointed to by aec_shared_filter_state_t::x and aec_shared_filter_state_t::X. Also reused for main filter
     * aec_filter_state_t::T*/
    int32_t ref_input_frame[AEC_MAX_X_CHANNELS][AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING];
    /** Memory pointed to by aec_shared_filter_state_t::prev_y*/
    int32_t mic_prev_samples[AEC_MAX_Y_CHANNELS][AEC_PROC_FRAME_LENGTH - AEC_FRAME_ADVANCE];
    /** Memory pointed to by aec_shared_filter_state_t::prev_x*/
    int32_t ref_prev_samples[AEC_MAX_X_CHANNELS][AEC_PROC_FRAME_LENGTH - AEC_FRAME_ADVANCE];
    /** Memory pointed to by main filter aec_filter_state_t::h_hat. The main filter is stored in the time domain as
     * AEC_FRAME_ADVANCE length real 32bit phases.*/
    int32_t phase_pool_H_hat[(AEC_MAX_Y_CHANNELS*AEC_MAX_X_CHANNELS*AEC_MAIN_FILTER_PHASES) * AEC_FRAME_ADVANCE];
    /** Memory pointed to by aec_shared_filter_state_t::X_fifo, main filter aec_filter_state_t::X_fifo_1d and shadow
     * filter aec_filter_state_t::X_fifo_1d*/
    complex_s32_t phase_pool_X_fifo[(AEC_MAX_X_CHANNELS*AEC_MAIN_FILTER_PHASES) * AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by main filter aec_filter_state_t::Error and aec_filter_state_t::error*/
    complex_s32_t Error[AEC_MAX_Y_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by main filter aec_filter_state_t::Y_hat and aec_filter_state_t::y_hat*/
    complex_s32_t Y_hat[AEC_MAX_Y_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by main_filter aec_filter_state_t::X_energy*/
    int32_t X_energy[AEC_MAX_X_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by aec_shared_filter_state_t::sigma_XX*/
    int32_t sigma_XX[AEC_MAX_X_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by main filter aec_filter_state_t::inv_X_energy*/
    int32_t inv_X_energy[AEC_MAX_X_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by main filter aec_filter_state_t::overlap*/
    int32_t overlap[AEC_MAX_Y_CHANNELS][AEC_UNUSED_TAPS_PER_PHASE*2];
}aec_memory_pool_t;

/**
 * @brief aec_shadow_filt_memory_pool_t
 *
 * Memory pool for AEC shadow filter.
 *
 * This pool provides contiguous storage for all BFP (block floating-point from ``lib_xcore_math``) structures used by the AEC shadow filter (`aec_filter_state_t`).
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
 * which satisfies @ref aec_phase_pool_capacity. Note that this is not simply a matter of the runtime phase counts
 * being smaller than the compile-time ones: the phases drawn from a pool are not all the same size, so what has to
 * fit is the byte demand, which @ref AEC_MAIN_POOL_BYTES and @ref AEC_SHADOW_POOL_BYTES compute.
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
 * @ingroup aec_memory_pool
 */
typedef struct {
    /** Memory pointed to by shadow filter aec_filter_state_t::h_hat. Stored in the time domain as AEC_FRAME_ADVANCE
     * length real 32bit phases.*/
    int32_t phase_pool_H_hat[AEC_MAX_Y_CHANNELS * AEC_MAX_X_CHANNELS * AEC_SHADOW_FILTER_PHASES * AEC_FRAME_ADVANCE];
    /** Memory pointed to by shadow filter aec_filter_state_t::Error and aec_filter_state_t::error*/
    complex_s32_t Error[AEC_MAX_Y_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by shadow filter aec_filter_state_t::Y_hat and aec_filter_state_t::y_hat*/
    complex_s32_t Y_hat[AEC_MAX_Y_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by shadow filter aec_filter_state_t::T*/
    complex_s32_t T[AEC_MAX_X_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by shadow_filter aec_filter_state_t::X_energy*/
    int32_t X_energy[AEC_MAX_X_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by shadow_filter aec_filter_state_t::inv_X_energy*/
    int32_t inv_X_energy[AEC_MAX_X_CHANNELS][AEC_FD_FRAME_LENGTH];
    /** Memory pointed to by shadow filter aec_filter_state_t::overlap*/
    int32_t overlap[AEC_MAX_Y_CHANNELS][AEC_UNUSED_TAPS_PER_PHASE*2];
}aec_shadow_filt_memory_pool_t;

/**
 * @brief Bytes `aec_init()` takes from @ref aec_memory_pool_t for a runtime configuration.
 *
 * The pool is a linear allocation arena, so the only thing that has to hold for a runtime
 * configuration to be safe is that its total demand fits in `sizeof(aec_memory_pool_t)`. The
 * individual phase reservations in the struct are not separate budgets and cannot be checked
 * independently: an h_hat phase is AEC_FRAME_ADVANCE `int32_t` while an X_fifo phase is
 * AEC_FD_FRAME_LENGTH `complex_s32_t`, so trading one for the other changes the total.
 *
 * This is a compile time constant for compile time arguments, so it can be used in a
 * `_Static_assert` to check a fixed runtime configuration against the pool. It is the same quantity
 * `aec_priv_main_init()` asserts at runtime.
 *
 * @ingroup aec_memory_pool
 */
#define AEC_MAIN_POOL_BYTES(num_y, num_x, num_main_phases) ( \
      ((num_y) + (num_x)) * (AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING) * sizeof(int32_t) \
    + ((num_y) + (num_x)) * (AEC_PROC_FRAME_LENGTH - AEC_FRAME_ADVANCE) * sizeof(int32_t) \
    + (num_y) * (num_x) * (num_main_phases) * AEC_FRAME_ADVANCE * sizeof(int32_t) \
    + (num_x) * (num_main_phases) * AEC_FD_FRAME_LENGTH * sizeof(complex_s32_t) \
    + 2 * (num_y) * AEC_FD_FRAME_LENGTH * sizeof(complex_s32_t) \
    + 3 * (num_x) * AEC_FD_FRAME_LENGTH * sizeof(int32_t) \
    + (num_y) * (AEC_UNUSED_TAPS_PER_PHASE * 2) * sizeof(int32_t) )

/**
 * @brief Bytes `aec_init()` takes from @ref aec_shadow_filt_memory_pool_t for a runtime
 * configuration. See @ref AEC_MAIN_POOL_BYTES.
 *
 * @ingroup aec_memory_pool
 */
#define AEC_SHADOW_POOL_BYTES(num_y, num_x, num_shadow_phases) ( \
      (num_y) * (num_x) * (num_shadow_phases) * AEC_FRAME_ADVANCE * sizeof(int32_t) \
    + (2 * (num_y) + (num_x)) * AEC_FD_FRAME_LENGTH * sizeof(complex_s32_t) \
    + 2 * (num_x) * AEC_FD_FRAME_LENGTH * sizeof(int32_t) \
    + (num_y) * (AEC_UNUSED_TAPS_PER_PHASE * 2) * sizeof(int32_t) )

/* The two formulas above restate the allocation sequence in aec_priv_main_init() and
 * aec_priv_shadow_init(). These assertions tie them to the pool definitions: at the compile time
 * configuration a pool is allocated in full, so the formula has to reproduce the struct exactly.
 * A member added to a pool, or an allocation resized, without the formula being updated therefore
 * fails to build here, rather than silently making every check built on these formulas optimistic. */
_Static_assert(AEC_MAIN_POOL_BYTES(AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS, AEC_MAIN_FILTER_PHASES)
                == sizeof(aec_memory_pool_t),
        "AEC_MAIN_POOL_BYTES() no longer matches aec_memory_pool_t - update it to match the "
        "allocations made by aec_priv_main_init()");
_Static_assert(AEC_SHADOW_POOL_BYTES(AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS, AEC_SHADOW_FILTER_PHASES)
                == sizeof(aec_shadow_filt_memory_pool_t),
        "AEC_SHADOW_POOL_BYTES() no longer matches aec_shadow_filt_memory_pool_t - update it to "
        "match the allocations made by aec_priv_shadow_init()");
#endif
