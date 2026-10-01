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

//The gather and scatter below move a whole complex element - a pair of taps - at a time, so that one load and one
//store moves each one rather than a pair of each. That needs the pair to be aligned to its own width, which holds
//for every buffer they are used on: aec_state_t declares both AEC memory pools DWORD_ALIGNED and every allocation
//ahead of h_hat in them is a whole number of double words, and the FFT scratch buffers here are declared
//DWORD_ALIGNED.
//
//h_hat stores 16 bit taps while the transforms work at 32 bit, so the two directions are not symmetric. The scatter
//widens as it goes - a pair of taps is one word in h_hat and a double word in the transform buffer - putting each
//tap in the top half of its slot, which is the widening that costs no instructions. The gather leaves the delta at
//32 bit and its caller narrows it afterwards, because narrowing a contiguous vector is a job for the VPU.
//
//On XS3 and VX4 these are hand written assembly - aec_h_hat_bitrev.S and aec_h_hat_bitrev_vx4b.S - because the
//load/store offsets a group needs run past the 0..11 immediate range both encodings allow and the compiler answers
//that by recomputing addresses, at about five instructions per element instead of two. The C below is the reference
//for what the assembly does, and is what other builds use.
#if defined(__XS3A__) || defined(__VX4B__)
//The assembly scatter writes only the slots h_hat stores, leaving the caller to zero the rest, because
//vect_s32_set() clears the whole vector with the VPU faster than the scatter can store the zeros itself.
void aec_h_hat_bitrev_scatter_kept(int32_t *dst, const int16_t *src);
void aec_h_hat_bitrev_scatter(int32_t *dst, const int16_t *src)
{
    vect_s32_set(dst, 0, AEC_PROC_FRAME_LENGTH);
    aec_h_hat_bitrev_scatter_kept(dst, src);
}

//The assembly hard codes the layout, so fail the build rather than mis-index if the frame sizes ever change it.
_Static_assert(AEC_H_HAT_BITREV_DROPPED == 8 && AEC_H_HAT_BITREV_GROUP == 16,
        "aec_h_hat_bitrev.S and aec_h_hat_bitrev_vx4b.S are written for the 8 group, 16 slot h_hat layout");
#else
typedef int64_t h_hat_tap_pair_t;

//Copy the taps h_hat stores out of a full bit-reversed index time domain vector, dropping the slots the gradient
//constraint zeroes. `src` may be the buffer `dst` points into; the copy only ever moves data towards the front.
void aec_h_hat_bitrev_gather(
        int32_t *dst_words,
        const int32_t *src_words)
{
    h_hat_tap_pair_t *dst = (h_hat_tap_pair_t*)dst_words;
    const h_hat_tap_pair_t *src = (const h_hat_tap_pair_t*)src_words;
    for(unsigned g=0; g<AEC_H_HAT_BITREV_DROPPED; g++) {
        for(unsigned i=0; i<AEC_H_HAT_BITREV_GROUP-1; i++) {
            dst[i] = src[2*i]; //the odd slot beside each one holds taps AEC_PROC_FRAME_LENGTH/2 onwards
        }
        dst += AEC_H_HAT_BITREV_GROUP-1; //past the dropped even slot, which holds the taps between
        src += 2*AEC_H_HAT_BITREV_GROUP; //AEC_FRAME_ADVANCE and AEC_PROC_FRAME_LENGTH/2, and its odd partner
    }
}

/**
 * Expand the taps h_hat stores into a full bit-reversed index time domain vector ready to be transformed in place,
 * widening each 16 bit tap into the top half of its 32 bit slot. That is a scaling by 2^16, which the caller accounts
 * for in the exponent it gives the transformed phase; it leaves the taps' headroom unchanged.
 * Every slot h_hat has no storage for is a tap the gradient constraint zeroes, so the whole vector is cleared first.
 */
void aec_h_hat_bitrev_scatter(
        int32_t *dst,
        const int16_t *src)
{
    vect_s32_set(dst, 0, AEC_PROC_FRAME_LENGTH);

    for(unsigned g=0; g<AEC_H_HAT_BITREV_DROPPED; g++) {
        for(unsigned i=0; i<AEC_H_HAT_BITREV_GROUP-1; i++) {
            dst[4*i]   = ((int32_t)src[2*i]) * (1 << 16);   //a stored pair of taps; the odd slot beside it holds taps
            dst[4*i+1] = ((int32_t)src[2*i+1]) * (1 << 16); //AEC_PROC_FRAME_LENGTH/2 onwards, and stays zero
        }
        dst += 4*AEC_H_HAT_BITREV_GROUP;     //past the dropped even slot, which holds the taps between
        src += 2*(AEC_H_HAT_BITREV_GROUP-1); //AEC_FRAME_ADVANCE and AEC_PROC_FRAME_LENGTH/2, and its odd partner
    }
}
#endif

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
        int32_t *scratch)
{
    //fft_dit_forward() requires 2 bits of headroom, do this on the compressed taps
    headroom_t hr = vect_s16_headroom(h_hat_ph->data, AEC_FRAME_ADVANCE);
    right_shift_t shr = 2 - (right_shift_t)hr;

    //Expand from compressed 16b to bit-reversed 32b taps
    aec_h_hat_bitrev_scatter(scratch, h_hat_ph->data);
    vect_s32_shl(scratch, scratch, AEC_PROC_FRAME_LENGTH, -shr);

    //Scatter shifts by 2^16 when going to 32b
    bfp_complex_s32_init(H_hat_ph, (complex_s32_t*)scratch, h_hat_ph->exp - 16 + shr, AEC_PROC_FRAME_LENGTH/2, 0);
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
 * This avoids a VX4 compiler bug related to stack frame handling when large scratch buffers are used.
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
    int32_t DWORD_ALIGNED h_fft_scratch[AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING];
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
    //the discarded taps are effectively zeroed.
    aec_h_hat_bitrev_gather((int32_t*)delta_scratch, (const int32_t*)delta_scratch);

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
