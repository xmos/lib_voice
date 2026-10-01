// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#if defined(__XS3A__)

/*
 * XS3 implementations of the h_hat bit-reversed gather and scatter described in aec_priv.h.
 *
 * Both move whole complex time domain elements - pairs of taps - so a single load and a single
 * store moves each one. That is the widest move available here: the VPU can only move contiguous
 * runs, and these are strided.
 *
 * h_hat holds 16 bit taps and the transforms work at 32 bit, so an element is one word on the
 * packed side and a double word on the strided side. The scatter therefore widens as it moves:
 * one ldw brings a pair of taps in, two ALU operations put each of them in the top half of its own
 * word, and one std writes the pair out. Placing them high rather than sign extending them low is
 * what makes the widening free - the mask and the shift each produce a finished word, and the
 * caller absorbs the resulting factor of 2^16 into the phase's exponent.
 *
 * The gather does not narrow. It leaves the delta at 32 bit for its caller to narrow with the VPU,
 * which can do a contiguous vector far quicker than this loop could do it an element at a time.
 *
 * The moves are blocked so that every load/store offset stays inside the 0..11 immediate range the
 * encoding allows, with the block base advanced from a register between blocks. That is what the C
 * these replace could not express - it answered the out of range offsets by recomputing addresses,
 * spending about five instructions per element instead of two.
 *
 * None of the moves here can use the VPU. A vector store can only place one element per store,
 * because each element moves a different distance, and on XS3 the masked store that would do it
 * (vstrpv) is a 32 bit instruction with no offset, so it costs as much as ldd/std and then needs
 * its address advanced as well. The scatter's widening moves each tap sideways within its word,
 * which no aligned vector access can do at all.
 *
 * What does matter is where the pointer and loop count updates go. ldd and std with an offset are
 * 32 bit instructions that fill both lanes of their bundle, but ldw with an offset is 16 bit and
 * leaves its other lane free, so the scatter does every one of its updates there and the gather,
 * which has no free lanes, pairs its updates with each other. Some of the updates share a bundle
 * with a load from the register they advance; both lanes of a bundle read their operands before
 * either writes, so the load still sees the old base.
 *
 *   void aec_h_hat_bitrev_scatter_kept(int32_t *dst, const int16_t *src);
 *   void aec_h_hat_bitrev_gather      (int32_t *dst, const int32_t *src);
 *
 * The layout constants below are the AEC_H_HAT_BITREV_* macros of aec_priv.h, which cannot be
 * included here. aec_l2_impl.c static asserts that they still agree.
 */

#define NGROUPS         8       // AEC_H_HAT_BITREV_DROPPED

// Block strides, in bytes. A group is 15 stored elements spread over 32 destination slots.
#define STEP_WIDE       96      // 12 double words: one 6 element block on the strided side
#define STEP_WIDE_LAST  64      //  8 double words: the rest of the group, past the dropped slot
#define STEP_NARROW     96      // 12 double words: the first 12 elements on the packed 32 bit side
#define STEP_NARROW_LAST 24     //  3 double words: the rest of the group
#define STEP_PACKED16   48      // 12 words: the first 12 elements on the packed 16 bit side
#define STEP_PACKED16_LAST 12   //  3 words: the rest of the group

.text
.issue_mode dual

/* ------------------------------------------------------------------------------------------- */

#define FUNCTION_NAME aec_h_hat_bitrev_scatter_kept

.globl FUNCTION_NAME
.type FUNCTION_NAME,@function
.align 16
.cc_top FUNCTION_NAME.function,FUNCTION_NAME

// r0 = dst (AEC_PROC_FRAME_LENGTH words, double word aligned), r1 = src (AEC_FRAME_ADVANCE 16 bit
// taps, word aligned). Only the kept double words are written, so the caller has to have zeroed
// the destination first - vect_s32_set() does that with the VPU, faster than this can store them.
//
// SCATTER_ELEMENT moves one element: r4 holds the source word, whose low half is the even tap and
// whose high half is the odd tap. r5 takes the even tap shifted up into the top of its word, and
// r4 is masked down to the odd tap already sitting in the top of its own. std then writes r5 to
// the lower address of the destination double word and r4 to the higher one, which is the order
// the two taps go in. The shift and the mask both read r4 before either writes, so they pair.
// SCATTER_ELEMENT_AND also puts one of the loop's pointer or count updates in the lane the ldw
// leaves free.
#define SCATTER_ELEMENT_AND(src_off, dst_off, ...)                                             \
    {   __VA_ARGS__                             ;   ldw r4, r1[src_off]                     }; \
    {   shl r5, r4, 16                          ;   and r4, r4, r2                          }; \
    {   std r4, r5, r0[dst_off]                                                              }
#define SCATTER_ELEMENT(src_off, dst_off)                                                      \
    {   ldw r4, r1[src_off]                                                                 }; \
    {   shl r5, r4, 16                          ;   and r4, r4, r2                          }; \
    {   std r4, r5, r0[dst_off]                                                              }

#define NSTACKWORDS     6

FUNCTION_NAME:
    {   dualentsp NSTACKWORDS                                                               }
    {   std r4, r5, sp[0]                                                                   }
    {   std r6, r7, sp[1]                                                                   }
    {   std r8, r9, sp[2]                                                                   }

    // STEP_WIDE and STEP_WIDE_LAST are past the 0..63 a bundled ldc reaches, so they go alone.
    {   ldc r6, STEP_WIDE                                                                   }
    {   ldc r7, STEP_WIDE_LAST                                                              }
    {   ldc r8, STEP_PACKED16                   ;   ldc r9, STEP_PACKED16_LAST              }
    {   mkmsk r2, 16                            ;   ldc r3, NGROUPS                         }
    // Each group starts by stepping dst on past the group before, so start it one step back.
    {   shl r2, r2, 16                          ;   sub r0, r0, r7                          }
.align 4
.L_scatter_group:
    // Elements 0..11 load from one src base, since ldw reaches 12 words. dst moves on every 6.
    SCATTER_ELEMENT_AND(0, 0, add r0, r0, r7);
    SCATTER_ELEMENT(1, 2);
    SCATTER_ELEMENT(2, 4);
    SCATTER_ELEMENT(3, 6);
    SCATTER_ELEMENT(4, 8);
    SCATTER_ELEMENT(5, 10);

    SCATTER_ELEMENT_AND(6, 0, add r0, r0, r6);
    SCATTER_ELEMENT(7, 2);
    SCATTER_ELEMENT(8, 4);
    SCATTER_ELEMENT(9, 6);
    SCATTER_ELEMENT(10, 8);
    SCATTER_ELEMENT_AND(11, 10, add r1, r1, r8);

    SCATTER_ELEMENT_AND(0, 0, add r0, r0, r6);
    SCATTER_ELEMENT_AND(1, 2, sub r3, r3, 1);
    SCATTER_ELEMENT_AND(2, 4, add r1, r1, r9);
    {   bt r3, .L_scatter_group                                                             }

    {   ldd r8, r9, sp[2]                                                                   }
    {   ldd r6, r7, sp[1]                                                                   }
    {   ldd r4, r5, sp[0]                                                                   }
    {   retsp NSTACKWORDS                                                                   }

.cc_bottom FUNCTION_NAME.function
.set FUNCTION_NAME.nstackwords,NSTACKWORDS
.globl FUNCTION_NAME.nstackwords
.set FUNCTION_NAME.maxcores,1
.globl FUNCTION_NAME.maxcores
.set FUNCTION_NAME.maxtimers,0
.globl FUNCTION_NAME.maxtimers
.set FUNCTION_NAME.maxchanends,0
.globl FUNCTION_NAME.maxchanends

#undef FUNCTION_NAME
#undef NSTACKWORDS

/* ------------------------------------------------------------------------------------------- */

#define FUNCTION_NAME aec_h_hat_bitrev_gather

.globl FUNCTION_NAME
.type FUNCTION_NAME,@function
.align 16
.cc_top FUNCTION_NAME.function,FUNCTION_NAME

// r0 = dst (AEC_FRAME_ADVANCE words), r1 = src (AEC_PROC_FRAME_LENGTH words). Both double word
// aligned. src may be the buffer dst points into: the destination index of a move is never ahead
// of its source index and the moves run in increasing order, so an in place gather is safe.
//
// ldd and std leave no lane free, so the updates pair with each other instead. src moves on every
// 6 elements and dst, whose offsets step half as far, on every 12.
// Those two strides are the same, so r6 serves both.
#if STEP_NARROW != STEP_WIDE
#error the gather advances src and dst by the same register
#endif
#define NSTACKWORDS     4

FUNCTION_NAME:
    {   dualentsp NSTACKWORDS                                                               }
    {   std r4, r5, sp[0]                                                                   }
    {   std r6, r7, sp[1]                                                                   }

    // STEP_WIDE and STEP_WIDE_LAST are past the 0..63 a bundled ldc reaches, so they go alone.
    {   ldc r6, STEP_WIDE                                                                   }
    {   ldc r7, STEP_WIDE_LAST                                                              }
    {   ldc r2, STEP_NARROW_LAST                ;   ldc r3, NGROUPS                         }
.align 4
.L_gather_group:
    {   ldd r4, r5, r1[0]                                                                   }
    {   std r4, r5, r0[0]                                                                   }
    {   ldd r4, r5, r1[2]                                                                   }
    {   std r4, r5, r0[1]                                                                   }
    {   ldd r4, r5, r1[4]                                                                   }
    {   std r4, r5, r0[2]                                                                   }
    {   ldd r4, r5, r1[6]                                                                   }
    {   std r4, r5, r0[3]                                                                   }
    {   ldd r4, r5, r1[8]                                                                   }
    {   std r4, r5, r0[4]                                                                   }
    {   ldd r4, r5, r1[10]                                                                  }
    {   std r4, r5, r0[5]                                                                   }
    {   add r1, r1, r6                          ;   sub r3, r3, 1                           }

    {   ldd r4, r5, r1[0]                                                                   }
    {   std r4, r5, r0[6]                                                                   }
    {   ldd r4, r5, r1[2]                                                                   }
    {   std r4, r5, r0[7]                                                                   }
    {   ldd r4, r5, r1[4]                                                                   }
    {   std r4, r5, r0[8]                                                                   }
    {   ldd r4, r5, r1[6]                                                                   }
    {   std r4, r5, r0[9]                                                                   }
    {   ldd r4, r5, r1[8]                                                                   }
    {   std r4, r5, r0[10]                                                                  }
    {   ldd r4, r5, r1[10]                                                                  }
    {   std r4, r5, r0[11]                                                                  }
    {   add r1, r1, r6                          ;   add r0, r0, r6                          }

    {   ldd r4, r5, r1[0]                                                                   }
    {   std r4, r5, r0[0]                                                                   }
    {   ldd r4, r5, r1[2]                                                                   }
    {   std r4, r5, r0[1]                                                                   }
    {   ldd r4, r5, r1[4]                                                                   }
    {   std r4, r5, r0[2]                                                                   }
    {   add r1, r1, r7                          ;   add r0, r0, r2                          }
    {   bt r3, .L_gather_group                                                              }

    {   ldd r6, r7, sp[1]                                                                   }
    {   ldd r4, r5, sp[0]                                                                   }
    {   retsp NSTACKWORDS                                                                   }

.cc_bottom FUNCTION_NAME.function
.set FUNCTION_NAME.nstackwords,NSTACKWORDS
.globl FUNCTION_NAME.nstackwords
.set FUNCTION_NAME.maxcores,1
.globl FUNCTION_NAME.maxcores
.set FUNCTION_NAME.maxtimers,0
.globl FUNCTION_NAME.maxtimers
.set FUNCTION_NAME.maxchanends,0
.globl FUNCTION_NAME.maxchanends

#endif // defined(__XS3A__)
