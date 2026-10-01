// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "aec.h"
#include "aec_priv.h"

//AEC level 2
//The adaptive filter is stored in the time domain to save memory. h_hat is transformed to the frequency domain
//on the fly, one phase at a time, during the Error and Y_hat calculation.
//
//The taps are held in bit-reversed index order so that neither of those per-phase transforms has to run an index
//bit-reversal pass.

//The mono FFT packs the real time domain signal into complex pairs to reduce the FFT size. The
//gather and scatter below move a whole complex element at a time. That needs the pair to be aligned
//to its own width, which holds for every buffer they are used on: aec_state_t declares the AEC
//memory pool DWORD_ALIGNED and aec_init() rounds every allocation from it up to a whole number of
//double words (AEC_POOL_ALIGN()), and the FFT scratch buffers here are declared DWORD_ALIGNED.
//
//h_hat stores 16 bit taps while the transforms work at 32 bit, so the two directions are not symmetric. The scatter
//widens as it goes - a pair of taps is one word in h_hat and a double word in the transform buffer - putting each
//tap in the top half of its slot, which is the widening that costs no instructions. The gather leaves the delta at
//32 bit and its caller narrows it afterwards, because narrowing a contiguous vector is a job for the VPU.

//On XS3 these moves cost more in address arithmetic than in the loads and stores themselves, and that shapes the code
//below. Each pair of taps moves as a single int64_t or uint32_t rather than as a struct. Each group of kept slots is
//written out as blocks small enough for every offset to fit the load/store immediates. In the scatter the step between
//blocks is a run time argument of a non-static function: given a constant, the compiler folds the steps into offsets
//outside the immediate range and rebuilds each address. The same shape also compiles well for VX4.
_Static_assert(AEC_H_HAT_BITREV_GROUP == 16, "the h_hat gather and scatter below move 15 kept slots per group");

#if defined(__XS3A__)
#define AEC_DUAL_ISSUE __attribute__((dual_issue))
#else
#define AEC_DUAL_ISSUE
#endif

static inline void gather_5_pairs(int64_t *d, const int64_t *s)
{
    d[0] = s[0];
    d[1] = s[2];
    d[2] = s[4];
    d[3] = s[6];
    d[4] = s[8];
}

//Copy the taps h_hat stores out of a full bit-reversed index time domain vector, dropping the slots the gradient
//constraint zeroes. `src` may be the buffer `dst` points into; the copy only ever moves data towards the front, and
//reads each slot before anything is written over it. Not inlined, so that it keeps its dual issue attribute.
__attribute__((noinline)) AEC_DUAL_ISSUE
void aec_h_hat_bitrev_gather(
        complex_s32_t *dst,
        const complex_s32_t *src)
{
    //Every even slot is kept; the odd slot beside each holds taps AEC_PROC_FRAME_LENGTH/2 onwards. A block of five
    //kept slots spans ten. Each group then skips its dropped even slot, which holds the taps between AEC_FRAME_ADVANCE
    //and AEC_PROC_FRAME_LENGTH/2, and that slot's odd partner.
    int64_t *d = (int64_t*)dst;
    const int64_t *s = (const int64_t*)src;
    for(unsigned g=0; g<AEC_H_HAT_BITREV_DROPPED; g++) {
        gather_5_pairs(d, s);      s += 10;
        gather_5_pairs(d + 5, s);  s += 10;
        gather_5_pairs(d + 10, s); s += 12;
        d += 15;
    }
}

//Widen one pair of 16 bit taps, the even one in the low half of `w`, into the top halves of two words.
static inline void widen_pair(int32_t *d, uint32_t w)
{
    d[0] = w << 16;
    d[1] = w & 0xFFFF0000;
}

static inline void widen_3_pairs(int32_t *d, const uint32_t *s)
{
    widen_pair(&d[0], s[0]);
    widen_pair(&d[4], s[1]);
    widen_pair(&d[8], s[2]);
}

__attribute__((noinline)) AEC_DUAL_ISSUE
void aec_h_hat_bitrev_scatter_blocks(
        complex_s32_t *dst,
        const complex_s16_t *src,
        unsigned block_step)
{
    vect_complex_s32_set(dst, 0, 0, AEC_PROC_FRAME_LENGTH/2);

    int32_t *d = (int32_t*)dst;
    const uint32_t *s = (const uint32_t*)src;
    for(unsigned g=0; g<AEC_H_HAT_BITREV_DROPPED; g++) {
        widen_3_pairs(d, s);      d += block_step;
        widen_3_pairs(d, s + 3);  d += block_step;
        widen_3_pairs(d, s + 6);  d += block_step;
        widen_3_pairs(d, s + 9);  d += block_step;
        widen_3_pairs(d, s + 12); d += block_step + 4;
        s += 15;
    }
}

/**
 * Expand the taps h_hat stores into a full bit-reversed index time domain vector ready to be transformed in place,
 * widening each 16 bit tap into the top half of its 32 bit slot. That is a scaling by 2^16, which the caller accounts
 * for in the exponent it gives the transformed phase; it leaves the taps' headroom unchanged.
 * Every slot h_hat has no storage for is a tap the gradient constraint zeroes, so the whole vector is cleared first.
 */
void aec_h_hat_bitrev_scatter(
        complex_s32_t *dst,
        const complex_s16_t *src)
{
    //A block of three stored pairs fills three even slots, twelve words, stepping over their odd partners, which hold
    //taps AEC_PROC_FRAME_LENGTH/2 onwards and stay zero. Each group then steps over its dropped even slot and partner.
    aec_h_hat_bitrev_scatter_blocks(dst, src, 12);
}

unsigned aec_h_hat_tap_index(unsigned n)
{
    //Tap n is the (n&1)'th half of complex time domain element n/2, which the transform expects to find in the slot
    //whose index bit-reverses to n/2. Dropping every AEC_H_HAT_BITREV_GROUP'th slot from the stored phase shifts
    //that slot down by the number of dropped slots below it.
    const unsigned slot = n_bitrev(n >> 1, u32_ceil_log2(AEC_H_HAT_BITREV_SLOTS));
    return 2*(slot - (slot / AEC_H_HAT_BITREV_GROUP)) + (n & 1);
}

/**
 * Transform one bit-reversed time domain filter phase into its AEC_FD_FRAME_LENGTH spectrum, using `scratch` as the
 * in-place transform buffer. This mirrors bfp_fft_forward_mono() with the fft_index_bit_reversal() call dropped, since
 * h_hat is already stored in the index order the decimation-in-time forward transform wants.
 */
static void h_hat_forward_fft(
        bfp_complex_s32_t *H_hat_ph,
        const bfp_s16_t *h_hat_ph,
        complex_s32_t *scratch)
{
    //fft_dit_forward() requires 2 bits of headroom, do this on the compressed taps
    headroom_t hr = vect_s16_headroom(h_hat_ph->data, AEC_FRAME_ADVANCE);
    right_shift_t shr = 2 - (right_shift_t)hr;

    //Expand from compressed 16b to bit-reversed 32b taps
    aec_h_hat_bitrev_scatter(scratch, (const complex_s16_t*)h_hat_ph->data);
    if(shr) {
        vect_s32_shl((int32_t*)scratch, (const int32_t*)scratch, AEC_PROC_FRAME_LENGTH, -shr);
    }

    //Scatter shifts by 2^16 when going to 32b
    bfp_complex_s32_init(H_hat_ph, scratch, h_hat_ph->exp - 16 + shr, AEC_PROC_FRAME_LENGTH/2, 0);
    H_hat_ph->hr = hr + shr;

    // The coeffs are already bit reversed, so use DIT FFT
    fft_dit_forward(H_hat_ph->data, AEC_PROC_FRAME_LENGTH/2, &H_hat_ph->hr, &H_hat_ph->exp);
    fft_mono_adjust(H_hat_ph->data, AEC_PROC_FRAME_LENGTH, 0);
    bfp_complex_s32_headroom(H_hat_ph);
    bfp_fft_unpack_mono(H_hat_ph);
}

/**
 * Transform each filter phase in turn and accumulate X * H_hat into Y_hat over the requested chunk.
 * 
 * TODO: This avoids a prototype VX4 compiler bug related to stack frame handling when large scratch buffers are used.
 */
__attribute__((noinline))
static void aec_l2_accumulate_Y_hat(
        bfp_complex_s32_t *Y_hat,
        const bfp_complex_s32_t *X_fifo,
        const bfp_s16_t *h_hat,
        unsigned phases,
        unsigned start_offset,
        unsigned length)
{
    //Scratch to FFT the current filter phase from time domain to frequency domain
    complex_s32_t DWORD_ALIGNED h_fft_scratch[AEC_FD_FRAME_LENGTH];
    for(unsigned ph=0; ph<phases; ph++) {
        bfp_complex_s32_t H_hat_ph;
        h_hat_forward_fft(&H_hat_ph, &h_hat[ph], h_fft_scratch);

        bfp_complex_s32_t X_chunk, H_hat_chunk;
        bfp_complex_s32_init(&X_chunk, &X_fifo[ph].data[start_offset], X_fifo[ph].exp, length, 0);
        X_chunk.hr = X_fifo[ph].hr;
        bfp_complex_s32_init(&H_hat_chunk, &H_hat_ph.data[start_offset], H_hat_ph.exp, length, 0);
        H_hat_chunk.hr = H_hat_ph.hr;
        bfp_complex_s32_macc(Y_hat, &X_chunk, &H_hat_chunk);
    }
}

void aec_l2_calc_Error_and_Y_hat(
        bfp_complex_s32_t *Error,
        bfp_complex_s32_t *Y_hat,
        const bfp_complex_s32_t *Y,
        const bfp_complex_s32_t *X_fifo,
        const bfp_s16_t *h_hat,
        unsigned num_x_channels,
        unsigned num_phases,
        unsigned start_offset,
        unsigned length,
        int32_t bypass_enabled)
{
    if(!length) {
        return;
    }
    if(bypass_enabled) { //Copy Y into Error. Set Y_hat to 0
        vpu_memcpy(Error->data, &Y->data[start_offset], length*sizeof(complex_s32_t));
        Error->exp = Y->exp;
        Error->hr = Y->hr;

        vect_complex_s32_set(Y_hat->data, 0, 0, length);
        Y_hat->exp = AEC_ZEROVAL_EXP;
        Y_hat->hr = AEC_ZEROVAL_HR;
    }
    else {
        aec_l2_accumulate_Y_hat(Y_hat, X_fifo, h_hat, num_x_channels * num_phases, start_offset, length);

        bfp_complex_s32_t Y_chunk;
        bfp_complex_s32_init(&Y_chunk, &Y->data[start_offset], Y->exp, length, 0);
        Y_chunk.hr = Y->hr;
        bfp_complex_s32_sub(Error, &Y_chunk, Y_hat);
    }
}

/**
 * Time domain filter adaption. The gradient constraint is applied simply by keeping only the taps
 * of the inverse FFT of T*conj(X) that h_hat has storage for (the rest would wrap in the circular
 * convolution).
 */
void aec_l2_adapt_plus_ifft(
        bfp_s16_t *h_hat_ph,
        const bfp_complex_s32_t *X_fifo_ph,
        const bfp_complex_s32_t *T_ph
        )
{
    complex_s32_t DWORD_ALIGNED delta_scratch[AEC_FD_FRAME_LENGTH];
    bfp_complex_s32_t prod;
    bfp_complex_s32_init(&prod, delta_scratch, AEC_ZEROVAL_EXP, AEC_FD_FRAME_LENGTH, 0);
    //prod = T * conj(X)
    bfp_complex_s32_conj_mul(&prod, T_ph, X_fifo_ph);

    //delta_h = ifft(prod), computed in place over the delta_scratch buffer. This mirrors bfp_fft_inverse_mono(),
    //but skips the fft_index_bit_reversal() and stores the bit-reversed coefficients.
    bfp_fft_pack_mono(&prod);
    //fft_dif_inverse() requires 2 bits of headroom
    bfp_complex_s32_use_exponent(&prod, prod.exp - prod.hr + 2);
    fft_mono_adjust(prod.data, AEC_PROC_FRAME_LENGTH, 1);
    fft_dif_inverse(prod.data, AEC_PROC_FRAME_LENGTH/2, &prod.hr, &prod.exp);

    //Save the non-zero taps in bit-reversed order. The gradient constraint is applied as
    //the discarded taps are effectively zeroed. Although delta_scratch is complex, after the mono
    //inverse FFT it holds the real time domain delta, packed two taps to each complex element.
    aec_h_hat_bitrev_gather(delta_scratch, delta_scratch);

    // Narrow delta to 16-bit taps before adding to h_hat, this can be done inplace
    int32_t *delta_words = (int32_t*)delta_scratch;
    int16_t *delta_taps = (int16_t*)delta_scratch;

    // Calculate (h + delta) output exponent before we shift delta to 32b, so we can go directly to
    // the correct exponent
    const headroom_t delta_hr = vect_s32_headroom(delta_words, AEC_FRAME_ADVANCE);
    const exponent_t h_hat_min_exp = h_hat_ph->exp - (exponent_t)h_hat_ph->hr;
    const exponent_t delta_min_exp = prod.exp + 16 - (exponent_t)delta_hr;
    const exponent_t sum_exp = ((h_hat_min_exp > delta_min_exp) ? h_hat_min_exp : delta_min_exp) + 1;

    vect_s32_to_vect_s16(delta_taps, delta_words, AEC_FRAME_ADVANCE, sum_exp - prod.exp);

    // Update h_hat with delta
    h_hat_ph->hr = vect_s16_add(h_hat_ph->data, h_hat_ph->data, delta_taps, AEC_FRAME_ADVANCE,
                                sum_exp - h_hat_ph->exp, 0);
    h_hat_ph->exp = sum_exp;
}

void aec_l2_bfp_complex_s32_unify_exponent(
        bfp_complex_s32_t *chunks,
        int32_t *final_exp, uint32_t *final_hr,
        const uint32_t *mapping, uint32_t array_len,
        uint32_t desired_index,
        uint32_t min_headroom)
{
    *final_exp = INT_MIN;
    for(int i=0; i<array_len; i++) {
        if(((mapping == NULL) || (mapping[i] == desired_index)) && (chunks[i].length > 0)) {
            if((int32_t)(chunks[i].exp - chunks[i].hr + min_headroom) > *final_exp) {
                *final_exp = chunks[i].exp - chunks[i].hr + min_headroom;
            }
        }
    }
    *final_hr = INT_MAX; //smallest hr
    for(int i=0; i<array_len; i++) {
        if(((mapping == NULL) || (mapping[i] == desired_index)) && (chunks[i].length > 0)) {
           bfp_complex_s32_use_exponent(&chunks[i], *final_exp);
           *final_hr = (chunks[i].hr < *final_hr) ? chunks[i].hr : *final_hr;
        }
    }
}

void aec_l2_bfp_s32_unify_exponent(
        bfp_s32_t *chunks, int32_t *final_exp,
        uint32_t *final_hr,
        const uint32_t *mapping,
        uint32_t array_len,
        uint32_t desired_index,
        uint32_t min_headroom)
{
    *final_exp = INT_MIN; //find biggest exponent (fewest fraction bits)
    for(int i=0; i<array_len; i++) {
        if(((mapping == NULL) || (mapping[i] == desired_index)) && (chunks[i].length > 0)) {
            if((int32_t)(chunks[i].exp - chunks[i].hr + min_headroom) > *final_exp) {
                *final_exp = chunks[i].exp - chunks[i].hr + min_headroom;
            }
        }
    }
    *final_hr = INT_MAX; //smallest hr
    for(int i=0; i<array_len; i++) {
        if(((mapping == NULL) || (mapping[i] == desired_index)) && (chunks[i].length > 0)) {
           bfp_s32_use_exponent(&chunks[i], *final_exp);
           *final_hr = (chunks[i].hr < *final_hr) ? chunks[i].hr : *final_hr;
        }
    }
}
