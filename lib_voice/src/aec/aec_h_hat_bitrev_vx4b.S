// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#if defined(__VX4B__)

/*
 * VX4 implementations of the h_hat bit-reversed gather and scatter described in aec_priv.h. They
 * are the same moves as the XS3 ones in aec_h_hat_bitrev.S - see there for why they move whole
 * complex elements, and why the scatter widens while the gather does not narrow.
 *
 * The encoding constraints are also the same. xm.lddi and xm.stdi take a 0..11 double word
 * immediate, and a packed lw takes a 0..11 word one, so the moves are again blocked 6 + 6 + 3
 * with the block bases advanced from registers between blocks. What differs is how the issue
 * slots fill:
 *
 * - xm.lddi and xm.stdi are 32 bit instructions and cannot go in a VLIW packet, so the gather is
 *   two unpaired instructions per element. Its pointer advances pair with each other instead,
 *   two to a packet between blocks.
 *
 * - A VLIW packet has one memory port, so the scatter's lw packs with one ALU operation and its
 *   shift and mask pack with each other, leaving a spare lane beside every lw. The pointer
 *   advances and the loop count all ride in those spare lanes, so the scatter costs three issue
 *   slots per element and one more per group for the branch.
 *
 * A packet reads all of its operands before either lane writes, which the scheduling below relies
 * on in two places: a pointer advance can share a packet with the last load from the old base, and
 * the shift and mask can both read the source word the mask overwrites.
 *
 * Only the reduced register file (a0..a3, s2..s8, t3) can be named in the packed and l3rus
 * encodings, which is why the constants live in callee saved registers rather than in the free
 * temporaries outside it.
 *
 *   void aec_h_hat_bitrev_scatter_kept(int32_t *dst, const int16_t *src);
 *   void aec_h_hat_bitrev_gather      (int32_t *dst, const int32_t *src);
 *
 * The layout constants below are the AEC_H_HAT_BITREV_* macros of aec_priv.h, which cannot be
 * included here. aec_l2_impl.c static asserts that they still agree.
 */

#define NGROUPS         8       // AEC_H_HAT_BITREV_DROPPED

// Block strides, in bytes. A group is 15 stored elements spread over 32 destination slots.
#define STEP_WIDE       96      // 12 double words: one block on the strided side
#define STEP_WIDE_LAST  64      //  8 double words: the rest of the group, past the dropped slot
#define STEP_NARROW_LAST 24     //  3 double words: the rest of the group
#define STEP_PACKED16   24      //  6 words: one block on the packed 16 bit side
#define STEP_PACKED16_LAST 12   //  3 words: the rest of the group

.text

/* ------------------------------------------------------------------------------------------- */

#define FUNCTION_NAME   aec_h_hat_bitrev_scatter_kept

// xm.entsp saves the link register in the top word of the frame, so five saved registers need a
// frame of eight words.
#define NSTACKWORDS     8

#define dst             a0
#define src             a1
#define odd             a2      // the source word, then the odd tap in the top of its own word
#define even            a3      // the even tap shifted up into the top of its word
#define mask            t3      // 0xFFFF0000
#define step_wide       s2
#define step_wide_last  s3
#define step_p16        s4
#define step_p16_last   s5
#define count           s6

// a0 = dst (AEC_PROC_FRAME_LENGTH words, double word aligned), a1 = src (AEC_FRAME_ADVANCE 16 bit
// taps, word aligned). Only the kept double words are written, so the caller has to have zeroed
// the destination first - vect_s32_set() does that with the VPU, faster than this can store them.
//
// SCATTER_ELEMENT moves one element in three issue slots: the lw with whatever `lane` carries
// beside it, the shift and mask together, then the store. The source word's low half is the even
// tap and its high half the odd tap; xm.stdi writes its first register to the lower address, which
// is where the even tap goes. Both offsets are in bytes, as the assembler takes them.
#define SCATTER_ELEMENT(src_off, dst_off, lane...)                                             \
  { lane                                ; lw odd, src_off(src)                  } ;            \
  { and odd, odd, mask                  ; slli even, odd, 16                    } ;            \
    xm.stdi even, odd, dst_off(dst)

.p2align 2
FUNCTION_NAME:
    xm.entsp (NSTACKWORDS)*4
    xm.stdsp s3, s2, 0
    xm.stdsp s5, s4, 8
  { nop                                 ; sw s6, 16(sp)                         }

    li step_wide, STEP_WIDE
    li step_wide_last, STEP_WIDE_LAST
    li step_p16, STEP_PACKED16
    li step_p16_last, STEP_PACKED16_LAST
    lui mask, 0xFFFF0
    li count, NGROUPS
    // Each group starts by advancing dst past the previous one, so start it a group step early.
    addi dst, dst, -STEP_WIDE_LAST

  .p2align 4
.L_scatter_group:
    // The src advance at the end of each block shares the packet of that block's last load, which
    // reads the old base. The dst advance goes beside the first load of the next block, after the
    // last store from the old one - for the first block, the next group's.
    SCATTER_ELEMENT( 0,   0, add dst, dst, step_wide_last);
    SCATTER_ELEMENT( 4,  16, addi count, count, -1);
    SCATTER_ELEMENT( 8,  32, nop);
    SCATTER_ELEMENT(12,  48, nop);
    SCATTER_ELEMENT(16,  64, nop);
    SCATTER_ELEMENT(20,  80, add src, src, step_p16);

    SCATTER_ELEMENT( 0,   0, add dst, dst, step_wide);
    SCATTER_ELEMENT( 4,  16, nop);
    SCATTER_ELEMENT( 8,  32, nop);
    SCATTER_ELEMENT(12,  48, nop);
    SCATTER_ELEMENT(16,  64, nop);
    SCATTER_ELEMENT(20,  80, add src, src, step_p16);

    SCATTER_ELEMENT( 0,   0, add dst, dst, step_wide);
    SCATTER_ELEMENT( 4,  16, nop);
    SCATTER_ELEMENT( 8,  32, add src, src, step_p16_last);

    // xm.bt only reaches +-64 half words, less than a group, so this needs the 32 bit branch.
    bnez count, .L_scatter_group

    xm.lddsp s3, s2, 0
    xm.lddsp s5, s4, 8
  { nop                                 ; lw s6, 16(sp)                         }
    xm.retsp (NSTACKWORDS)*4

.L_scatter_end:

.globl FUNCTION_NAME
.type FUNCTION_NAME,@function
.resource_const FUNCTION_NAME, "stack_frame_bytes", (NSTACKWORDS)*4
.resource_list_empty FUNCTION_NAME, "tail_callees"
.resource_list_empty FUNCTION_NAME, "callees"
.resource_list_empty FUNCTION_NAME, "parallel_callees"
.size FUNCTION_NAME, .L_scatter_end - FUNCTION_NAME

#undef FUNCTION_NAME
#undef NSTACKWORDS
#undef dst
#undef src
#undef odd
#undef even
#undef mask
#undef step_wide
#undef step_wide_last
#undef step_p16
#undef step_p16_last
#undef count

/* ------------------------------------------------------------------------------------------- */

#define FUNCTION_NAME   aec_h_hat_bitrev_gather

// Three saved registers and the link register fill a four word frame exactly.
#define NSTACKWORDS     4

#define dst             a0
#define src             a1
#define lo              a2
#define hi              a3
#define count           t3
#define step_wide       s2
#define step_wide_last  s3
#define step_narrow_last s4

// a0 = dst (AEC_FRAME_ADVANCE words), a1 = src (AEC_PROC_FRAME_LENGTH words). Both double word
// aligned. src may be the buffer dst points into: the destination index of a move is never ahead
// of its source index and the moves run in increasing order, so an in place gather is safe.
//
// The dst base only moves twice a group, because the second block's destination offsets still fit
// the immediate from the first block's base. STEP_WIDE doubles as the dst advance after it, being
// both one block on the strided side and two on the packed side.
#define GATHER_ELEMENT(src_off, dst_off)                                                       \
    xm.lddi lo, hi, src_off(src) ;                                                             \
    xm.stdi lo, hi, dst_off(dst)

.p2align 2
FUNCTION_NAME:
    xm.entsp (NSTACKWORDS)*4
    xm.stdsp s3, s2, 0
  { nop                                 ; sw s4, 8(sp)                          }

    li step_wide, STEP_WIDE
    li step_wide_last, STEP_WIDE_LAST
    li step_narrow_last, STEP_NARROW_LAST
    li count, NGROUPS

  .p2align 4
.L_gather_group:
    GATHER_ELEMENT( 0,  0);
    GATHER_ELEMENT(16,  8);
    GATHER_ELEMENT(32, 16);
    GATHER_ELEMENT(48, 24);
    GATHER_ELEMENT(64, 32);
    GATHER_ELEMENT(80, 40);
  { add src, src, step_wide             ; addi count, count, -1                 }

    GATHER_ELEMENT( 0, 48);
    GATHER_ELEMENT(16, 56);
    GATHER_ELEMENT(32, 64);
    GATHER_ELEMENT(48, 72);
    GATHER_ELEMENT(64, 80);
    GATHER_ELEMENT(80, 88);
  { add src, src, step_wide             ; add dst, dst, step_wide               }

    GATHER_ELEMENT( 0,  0);
    GATHER_ELEMENT(16,  8);
    GATHER_ELEMENT(32, 16);
  { add src, src, step_wide_last        ; add dst, dst, step_narrow_last        }
    bnez count, .L_gather_group        // a group is out of xm.bt's reach

    xm.lddsp s3, s2, 0
  { nop                                 ; lw s4, 8(sp)                          }
    xm.retsp (NSTACKWORDS)*4

.L_gather_end:

.globl FUNCTION_NAME
.type FUNCTION_NAME,@function
.resource_const FUNCTION_NAME, "stack_frame_bytes", (NSTACKWORDS)*4
.resource_list_empty FUNCTION_NAME, "tail_callees"
.resource_list_empty FUNCTION_NAME, "callees"
.resource_list_empty FUNCTION_NAME, "parallel_callees"
.size FUNCTION_NAME, .L_gather_end - FUNCTION_NAME

#endif // defined(__VX4B__)
