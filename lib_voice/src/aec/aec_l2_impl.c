// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "aec.h"
#include "aec_priv.h"

//AEC level 2
void aec_l2_calc_Error_and_Y_hat(
        bfp_complex_s32_t *Error,
        bfp_complex_s32_t *Y_hat,
        const bfp_complex_s32_t *Y,
        const bfp_complex_s32_t *X_fifo,
        const bfp_complex_s32_t *H_hat,
        unsigned num_x_channels,
        unsigned num_phases,
        unsigned start_offset,
        unsigned length,
        int32_t bypass_enabled)
{
    if(!length) {
        //printf("0 length\n");
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
        uint32_t phases = num_x_channels * num_phases;
        for(unsigned ph=0; ph<phases; ph++) {
            //create input chunks
            bfp_complex_s32_t X_chunk, H_hat_chunk;
            bfp_complex_s32_init(&X_chunk, &X_fifo[ph].data[start_offset], X_fifo[ph].exp, length, 0); //Not recalculating headroom here to make sure outputs are bitexact irrespective of the length this function is called for.
            X_chunk.hr = X_fifo[ph].hr;
            bfp_complex_s32_init(&H_hat_chunk, &H_hat[ph].data[start_offset], H_hat[ph].exp, length, 0);
            H_hat_chunk.hr = H_hat[ph].hr;
            bfp_complex_s32_macc(Y_hat, &X_chunk, &H_hat_chunk);
        }

        bfp_complex_s32_t Y_chunk;
        bfp_complex_s32_init(&Y_chunk, &Y->data[start_offset], Y->exp, length, 0);
        Y_chunk.hr = Y->hr;
        bfp_complex_s32_sub(Error, &Y_chunk, Y_hat);
    }
}

void aec_l2_adapt_plus_fft_gc(
        bfp_complex_s32_t *H_hat_ph,
        const bfp_complex_s32_t *X_fifo_ph,
        const bfp_complex_s32_t *T_ph
        )
{
    bfp_complex_s32_conj_macc(H_hat_ph, T_ph, X_fifo_ph);
    bfp_fft_pack_mono(H_hat_ph);
    bfp_complex_s32_gradient_constraint_mono(H_hat_ph, 240);
    bfp_fft_unpack_mono(H_hat_ph);
}

//AEC level 2, time domain filter variants
//The AEC adaptive filter is stored in the time domain to save memory. h_hat is transformed to the frequency domain
//on the fly, one phase at a time, during the Error and Y_hat calculation.
//
//The taps are held in bit-reversed index order so that neither of those per-phase transforms has to run an index
//bit-reversal pass - see the h_hat bit-reversed storage layout notes in aec_priv.h for how that order is laid out.

//The gather and scatter below move a whole complex element - a pair of taps - at a time, so that one load and one
//store moves each one rather than a pair of each. That needs the pair to be aligned to its own width, which holds
//for every buffer they are used on: aec_state_t declares both AEC memory pools DWORD_ALIGNED and every allocation
//ahead of h_hat in them is a whole number of double words, and the FFT scratch buffers here are declared
//DWORD_ALIGNED. A strided move like this is the one thing vpu_memcpy() cannot do, so a pair is as wide as it goes.
//
//h_hat stores 16 bit taps while the transforms work at 32 bit, so the two directions are not symmetric. The scatter
//widens as it goes - a pair of taps is one word in h_hat and a double word in the transform buffer - putting each
//tap in the top half of its slot, which is the widening that costs no instructions. The gather leaves the delta at
//32 bit and its caller narrows it afterwards, because narrowing a contiguous vector is a job for the VPU.
//
//On XS3 these are the hand written aec_h_hat_bitrev.S, because the load/store offsets a group needs run past the
//0..11 immediate range the encoding allows and the compiler answers that by recomputing addresses, at about five
//instructions per element instead of two. The C below is the reference for what the assembly does, and is what
//non-XS3 builds use.
#if defined(__XS3A__)
void aec_h_hat_bitrev_gather(int32_t *dst, const int32_t *src);
//The assembly scatter writes only the slots h_hat stores, leaving the caller to zero the rest, because
//vect_s32_set() clears the whole vector with the VPU faster than the scatter can store the zeros itself.
void aec_h_hat_bitrev_scatter_kept(int32_t *dst, const int16_t *src);
static inline void aec_h_hat_bitrev_scatter(int32_t *dst, const int16_t *src)
{
    vect_s32_set(dst, 0, AEC_PROC_FRAME_LENGTH);
    aec_h_hat_bitrev_scatter_kept(dst, src);
}

//The assembly hard codes the layout, so fail the build rather than mis-index if the frame sizes ever change it.
_Static_assert(AEC_H_HAT_BITREV_DROPPED == 8 && AEC_H_HAT_BITREV_GROUP == 16,
        "aec_h_hat_bitrev.S is written for the 8 group, 16 slot h_hat layout");
#else
typedef int64_t h_hat_tap_pair_t;

//Copy the taps h_hat stores out of a full bit-reversed index time domain vector, dropping the slots the gradient
//constraint zeroes. `src` may be the buffer `dst` points into; the copy only ever moves data towards the front.
static void aec_h_hat_bitrev_gather(
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

//Expand the taps h_hat stores into a full bit-reversed index time domain vector ready to be transformed in place,
//widening each 16 bit tap into the top half of its 32 bit slot. That is a scaling by 2^16, which the caller accounts
//for in the exponent it gives the transformed phase; it leaves the taps' headroom unchanged.
//Every slot h_hat has no storage for is a tap the gradient constraint zeroes, so the whole vector is cleared first
//- vect_s32_set() does that a vector at a time, faster than storing the zeros one slot at a time here would.
static void aec_h_hat_bitrev_scatter(
        int32_t *dst,
        const int16_t *src)
{
    vect_s32_set(dst, 0, AEC_PROC_FRAME_LENGTH);

    for(unsigned g=0; g<AEC_H_HAT_BITREV_DROPPED; g++) {
        for(unsigned i=0; i<AEC_H_HAT_BITREV_GROUP-1; i++) {
            dst[4*i]   = ((int32_t)src[2*i]) << 16;   //a stored pair of taps; the odd slot beside it holds taps
            dst[4*i+1] = ((int32_t)src[2*i+1]) << 16; //AEC_PROC_FRAME_LENGTH/2 onwards, and stays zero
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

//Transform one bit-reversed time domain filter phase into its AEC_FD_FRAME_LENGTH spectrum, using `scratch` as the
//in-place transform buffer. This mirrors bfp_fft_forward_mono() with the fft_index_bit_reversal() call dropped, since
//h_hat is already stored in the index order the decimation-in-time forward transform wants.
static void h_hat_forward_fft(
        bfp_complex_s32_t *H_hat_ph,
        const bfp_s16_t *h_hat_ph,
        int32_t *scratch)
{
    //fft_dit_forward() requires 2 bits of headroom. Measure it over the stored taps rather than the expanded vector -
    //a quarter of the bytes read, since the taps the scatter zeroed cannot reduce it and the widening the scatter
    //does leaves it unchanged.
    headroom_t hr = vect_s16_headroom(h_hat_ph->data, AEC_FRAME_ADVANCE);
    right_shift_t shr = 2 - (right_shift_t)hr;

    //The shift runs over the expanded vector, after the scatter, even though scaling the stored taps first would be
    //the same answer over a third of the elements - shifting a zero leaves a zero. Scaling first needs somewhere to
    //put the scaled taps that is neither h_hat (state) nor `scratch` (the scatter clears all of it before it writes),
    //so it costs an AEC_FRAME_ADVANCE buffer, and 480 bytes of stack on each of the threads that reach here is a far
    //worse trade than shifting the 272 extra slots the scatter zeroed.
    aec_h_hat_bitrev_scatter(scratch, h_hat_ph->data);
    vect_s32_shl(scratch, scratch, AEC_PROC_FRAME_LENGTH, -shr);

    //The scatter left each tap in the top half of its slot, so the expanded mantissas are 2^16 times the stored ones.
    bfp_complex_s32_init(H_hat_ph, (complex_s32_t*)scratch, h_hat_ph->exp - 16 + shr, AEC_PROC_FRAME_LENGTH/2, 0);
    H_hat_ph->hr = hr + shr;

    fft_dit_forward(H_hat_ph->data, AEC_PROC_FRAME_LENGTH/2, &H_hat_ph->hr, &H_hat_ph->exp);
    fft_mono_adjust(H_hat_ph->data, AEC_PROC_FRAME_LENGTH, 0);
    bfp_complex_s32_headroom(H_hat_ph);
    bfp_fft_unpack_mono(H_hat_ph);
}

void aec_l2_calc_Error_and_Y_hat_td(
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
        memcpy(Error->data, &Y->data[start_offset], length*sizeof(complex_s32_t));
        Error->exp = Y->exp;
        Error->hr = Y->hr;

        memset(Y_hat->data, 0, length*sizeof(complex_s32_t));
        Y_hat->exp = AEC_ZEROVAL_EXP;
        Y_hat->hr = AEC_ZEROVAL_HR;
    }
    else {
        //Scratch to hold one filter phase expanded from its bit-reversed storage into a full AEC_PROC_FRAME_LENGTH
        //time domain vector and, after the in-place FFT, its AEC_FD_FRAME_LENGTH spectrum. The full spectrum is
        //always computed so outputs are bitexact irrespective of the start_offset/length this function is called with.
        int32_t DWORD_ALIGNED h_fft_scratch[AEC_PROC_FRAME_LENGTH + AEC_FFT_PADDING];
        uint32_t phases = num_x_channels * num_phases;
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

        bfp_complex_s32_t Y_chunk;
        bfp_complex_s32_init(&Y_chunk, &Y->data[start_offset], Y->exp, length, 0);
        Y_chunk.hr = Y->hr;
        bfp_complex_s32_sub(Error, &Y_chunk, Y_hat);
    }
}

//Time domain filter adaption. The gradient constraint is applied simply by keeping only the taps of the inverse FFT
//of T*conj(X) that h_hat has storage for (the rest would wrap in the circular convolution).
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

    //delta_h = ifft(prod), computed in place over the delta_scratch buffer. This mirrors bfp_fft_inverse_mono() with
    //the fft_index_bit_reversal() call replaced by the use of the decimation-in-frequency inverse transform, which
    //leaves its time domain output in the bit-reversed index order h_hat is stored in.
    bfp_fft_pack_mono(&prod);
    //fft_dif_inverse() requires 2 bits of headroom
    bfp_complex_s32_use_exponent(&prod, prod.exp - prod.hr + 2);
    fft_mono_adjust(prod.data, AEC_PROC_FRAME_LENGTH, 1);
    //prod now describes the real time domain delta rather than a spectrum, so its exponent and headroom are updated
    //in place while its length is left alone - the gather below is what gives the result its length.
    fft_dif_inverse(prod.data, AEC_PROC_FRAME_LENGTH/2, &prod.hr, &prod.exp);

    //Compact the delta down to the taps h_hat stores, in place at the front of delta_scratch. Everything dropped
    //here is a tap the gradient constraint zeroes.
    aec_h_hat_bitrev_gather((int32_t*)delta_scratch, (const int32_t*)delta_scratch);

    //h_hat stores 16 bit taps, so the delta has to be narrowed to that depth before it can be added, and *where* the
    //narrowing happens decides how much of a small update survives. Narrow it straight to the exponent the sum will
    //be held at, rather than to its own full 16 bit scale and letting the add line it up afterwards.
    //
    //The difference is the rounding mode. vect_s32_to_vect_s16() rounds to nearest, whereas the shift vect_s16_add()
    //would otherwise apply to the delta is an arithmetic shift, which floors. Once the filter has converged the
    //per-tap update is a fraction of an LSB, and flooring turns every negative fraction into a whole -1 LSB while
    //leaving the positive ones at 0. That is not a rounding detail at this depth: roughly half the taps take a -1
    //every frame, so the filter walks steadily downwards instead of settling.
    int32_t *delta_words = (int32_t*)delta_scratch;
    int16_t *delta_taps = (int16_t*)delta_scratch;

    //The output exponent is the one vect_s16_add_prepare() would have chosen: whichever operand needs the most room,
    //plus a bit for the carry. A 32 bit delta with `hr` bits of headroom fills 16 bits at exponent prod.exp + 16 - hr.
    //The delta's headroom is measured over the gathered taps rather than taken from prod.hr, which is only a lower
    //bound once the gather has dropped more than half the vector, and it is exactly the bound that sets how much of
    //the update survives.
    const headroom_t delta_hr = vect_s32_headroom(delta_words, AEC_FRAME_ADVANCE);
    const exponent_t h_hat_min_exp = h_hat_ph->exp - (exponent_t)h_hat_ph->hr;
    const exponent_t delta_min_exp = prod.exp + 16 - (exponent_t)delta_hr;
    const exponent_t sum_exp = ((h_hat_min_exp > delta_min_exp) ? h_hat_min_exp : delta_min_exp) + 1;

    //Narrowing in place is safe: this reads 32 bits ahead of every 16 bits it writes.
    vect_s32_to_vect_s16(delta_taps, delta_words, AEC_FRAME_ADVANCE, sum_exp - prod.exp);

    //The delta already sits at the output exponent, so it is added unshifted and keeps the rounding above. Only
    //h_hat is shifted, and it is the operand a floored LSB cannot matter to.
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
