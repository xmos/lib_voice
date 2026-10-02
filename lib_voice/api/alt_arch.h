// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef ALT_ARCH_H
#define ALT_ARCH_H

/**
 * @defgroup alt_arch_defines   Alternating architecture #define constants
 */

/** @brief Enable the alternating architecture
 *
 * In the alternating architecture the AEC and the IC are never active at the same time. Based on activity on the
 * reference input:
 * - The AEC bypasses itself once the reference has been inactive for @ref HOLD_AEC_LIMIT_SECONDS, passing the
 *   microphone input unmodified and sample aligned to its output. It reports the held reference active flag
 *   through `aec_process_frame()`.
 * - The IC bypasses itself when the reference active flag passed to `ic_process_frame()` is set.
 *
 * The AEC processes 1 microphone channel. The second microphone channel bypasses the AEC and is passed
 * directly to the IC alongside the AEC output.
 *
 * Defaults to 0 (standard architecture, AEC and IC both always active). Define to 1 for every library and application
 * source file, for example in the application's CMakeLists.txt.
 *
 * @ingroup alt_arch_defines
 */
#ifndef ALT_ARCH_MODE
#define ALT_ARCH_MODE (0)
#endif

#endif
