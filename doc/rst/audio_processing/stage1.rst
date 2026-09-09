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
(reference energy, correlation factors, and activity flags) for downstream processing stages.

Two pipeline architectures are supported:

- **Standard Architecture**: Processes multiple microphone channels through both AEC and IC sequentially
- **Alternating Architecture**: Selectively enables AEC or IC based on reference signal presence,
  reducing memory requirements and enabling longer AEC filter tails

Signal Representation
---------------------

Stage1 processes audio on a frame-by-frame basis. Each frame consists of 15 ms of audio
(240 samples at 16 kHz), with input and output data in fixed-point 32-bit 1.31 format.

Inputs:

- Microphone (Y) channels: Up to 2 channels of microphone input
- Reference (X) channels: Up to 2 channels of reference (loudspeaker) input

Outputs:

- Echo-cancelled audio: Same number of channels as microphone input
- Metadata: Maximum reference energy, AEC correlation factors, reference activity flag

Standard Architecture
---------------------

In the Standard Architecture pipeline form, all the modules are enabled and called sequentially. This is shown in
:numref:`std_arch_pipeline`.

.. _std_arch_pipeline:

.. figure:: ../images/standard_arch_pipeline.drawio.svg
    :align: center

    The Standard Architecture Pipeline.

The AEC is configured for 2 mic input channels, 2 reference input channels, 10 phase main filter and a 5 phase shadow
filter. The IC is configured for 2 mic input channels, and 10 phase main filter.

When ADEC goes in delay estimation mode, the AEC gets reconfigured as a 1 mic input channel, 1
reference input channel, 30 main filter phases and no shadow filter, as described in the :ref:`adec_module` documentation.
During this, the IC remains active.

Once the new delay has been measured and the delay correction is
applied, the AEC gets configured back to its original configuration and starts adapting and cancellation.
The AEC stage generates the echo cancelled version of the mic input that is then sent for processing through the IC.

Alternating Architecture
------------------------

In this pipeline form, the AEC and the IC frame processing are selectively enabled and disabled
based on the presence of reference input signal. This is shown in :numref:`alt_arch_pipeline`.

Acoustic Echo Cancellation is performed only if activity is detected on the reference input
channels and disabled otherwise.

Interference Cancellation is performed only when AEC is disabled so in the absence of reference
channel activity and disabled otherwise.

This means that only 1 microphone signal requires processing by the AEC, reducing the number of
filters required. This saves memory, which can then be used to increase the AEC filter tail length.
This can improve AEC performance in more reverberant environments.

.. _alt_arch_pipeline:

.. figure:: ../images/alt_arch_stage1_all.drawio.svg
    :align: center

    The Alternating Architecture Stage 1.

When reference audio is detected, the AEC is enabled and the IC is disabled. The AEC processes one
microphone input to remove any echo from the reference signal. This is shown in :numref:`alt_arch_aec`. Note
the VNR output from the IC is still generated so that it can be used by the AGC.

.. _alt_arch_aec:

.. figure:: ../images/alt_arch_stage1_aec.drawio.svg
    :align: center

    The Alternating Architecture Stage 1 when the reference signal is present.

When no reference audio is detected, the AEC is disabled and the IC is enabled. The IC processes
both microphone inputs to remove any unwanted noise sources in the environment.
This is shown in :numref:`alt_arch_ic`.

.. _alt_arch_ic:

.. figure:: ../images/alt_arch_stage1_ic.drawio.svg
    :align: center

    The Alternating Architecture Stage 1 when no reference signal is present.

The AEC is configured for 1 mic input channel, 2 reference input channels, 15 phase main filter and a 5 phase shadow
filter giving an extended tail length for highly reverberant environments. 

When ADEC goes in delay estimation mode, the AEC gets reconfigured as a 1 mic input channel, 1
reference input channel, 30 main filter phases and no shadow filter, as described in the :ref:`adec_module` documentation.
In the absence of activity on the reference channels, when the AEC is disabled, the microphone input is copied directly to the output of the AEC.

Alternating architecture is disabled by default (see :c:macro:`ALT_ARCH_MODE`). To enable it, define ``ALT_ARCH_MODE`` to 1 in the application's CMakeLists.txt.

Fixed Delay (ADEC disabled)
-----------------------------------

The automatic delay estimation and correction performed by ADEC can be compiled out by defining
:c:macro:`STAGE1_DISABLE_ADEC` to 1 in the application's CMakeLists.txt. When disabled, Stage1 does
not run the delay estimator and never switches into delay estimation mode; it remains in its
regular AEC configuration and instead applies a fixed input delay through the delay buffer.

The application is responsible for setting the required delay after :c:func:`stage1_init()`, for
example by calling ``update_delay_samples()`` with the known microphone-to-reference delay in
samples (a positive value delays the microphone, a negative value delays the reference).

This is useful when the delay between the microphone and reference paths is known and fixed, as it
avoids the memory and compute cost of the delay estimator. It can be combined with either the
standard or the alternating architecture.

:c:macro:`STAGE1_DISABLE_ADEC` is 0 (ADEC enabled) by default.

Usage
-----

Before starting processing, Stage1 must be initialised by calling :c:func:`stage1_init()`.
This sets up internal state for the provided runtime AEC configurations and ADEC settings.

Once initialised, call :c:func:`stage1_process_frame()` for each input frame.

Refer to :ref:`pipeline_example` to see Stage1 integrated into an audio pipeline.

Parameters
----------

The key Stage 1 parameters are highlighted below:

* :c:macro:`REF_ACTIVE_THRESHOLD_DB` - This macro is used in alt arch mode, and sets the threshold
  for determining whether the reference signal is active, in decibels relative to full scale. 
  When the maximum value of the reference signal in a frame is below this threshold for 
  ``HOLD_AEC_LIMIT_SECONDS``, the AEC will be bypassed and the IC will be enabled
  If the reference signal is above this level, the AEC will be enabled and the IC bypassed.
  Note this parameter is shared with the AEC module.
* :c:macro:`HOLD_AEC_LIMIT_SECONDS` - This macro is used in alt arch mode, and sets the limit in 
  seconds for which AEC is kept enabled after detecting reference as inactive. This is to avoid
  toggling of AEC and IC when the reference signal is fluctuating around the reference active
  threshold. 
