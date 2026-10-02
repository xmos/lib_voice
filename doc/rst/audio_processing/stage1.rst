.. _stage1_module:

Pipeline Stage 1
================

Stage1 is typically the first stage in an audio pipeline. It orchestrates
delay alignment, Acoustic Echo Cancellation (AEC), and Adaptive Delay
Estimation/Canceller (ADEC), and propagates per-frame metadata downstream.

Overview
--------

The Stage1 component in ``lib_voice`` integrates the :ref:`aec_module`, :ref:`adec_module`,
and delay buffering to provide echo-cancelled audio with automatic delay correction.
Stage1 operates at a fixed 16 kHz sample rate.

Stage1 manages the transition between normal AEC operation and delay estimation mode,
applies delay corrections to maintain optimal AEC performance, and generates metadata
(reference energy, correlation factors, and the reference active flag) for downstream processing stages.

Stage1 supports both the standard and the alternating pipeline architectures described in
:ref:`aec_alt_arch`. The decision to bypass the AEC in the alternating architecture is made inside
the AEC, so Stage1 does not depend on the architecture.

Signal Representation
---------------------

Stage1 processes audio on a frame-by-frame basis. Each frame consists of 15 ms of audio
(240 samples at 16 kHz), with input and output data in fixed-point 32-bit 1.31 format.

Inputs:

- Microphone (Y) channels: :c:macro:`STAGE1_MAX_Y_CHANNELS` channels of microphone input
- Reference (X) channels: Up to 2 channels of reference (loudspeaker) input

Outputs:

- Echo-cancelled audio: :c:macro:`STAGE1_MAX_Y_CHANNELS` channels
- Metadata: Maximum reference energy, AEC correlation factors, reference active flag

Delay Alignment and Estimation
------------------------------

Every frame, Stage1 first delays either the microphone or the reference input through the
delay buffer, by the delay currently requested by ADEC. A positive delay delays the microphone, and a
negative delay delays the reference.

When ADEC requests a delay estimation cycle, the AEC is reconfigured as a 1 mic input channel, 1
reference input channel, 30 main filter phases and no shadow filter, as described in the
:ref:`adec_module` documentation. While the delay is being estimated, all output channels are the
delayed microphone input. Once the new delay has been measured and the delay correction is applied,
the AEC is configured back to its original configuration and starts adapting and cancelling again.

Channels Not Processed by the AEC
---------------------------------

Stage1 carries :c:macro:`STAGE1_MAX_Y_CHANNELS` microphone channels, which may be more than
the AEC is configured for. Microphone channels the AEC does not process are copied from the delayed
microphone input to the output. In the alternating architecture, for example, the AEC processes only
1 microphone channel, but Stage1 carries 2, because the IC needs both microphone channels while
the AEC is bypassed.

Usage
-----

Before starting processing, Stage1 must be initialised by calling :c:func:`stage1_init()`.
This sets up internal state for the provided runtime AEC configurations and ADEC settings.

Once initialised, call :c:func:`stage1_process_frame()` for each input frame. The output buffer must
not alias the microphone input, because the microphone input is needed to overwrite the output in some
cases.

Refer to :ref:`pipeline_example` to see Stage1 integrated into an audio pipeline.
