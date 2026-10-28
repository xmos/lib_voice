.. _alt_arch:


Reference Activity and Pipeline Architectures
=============================================

Every frame, :c:func:`aec_process_frame()` checks for activity on the reference input. The reference
is active when the maximum sample in the frame on any reference channel is above
:c:macro:`REF_ACTIVE_THRESHOLD_DB`. The AEC reports a held reference active flag through the
``ref_active_flag`` argument of :c:func:`aec_process_frame()`: the flag stays set until the reference
has been inactive for :c:macro:`HOLD_AEC_LIMIT_SECONDS`. Downstream stages use this flag, for example
the AGC for loss control and, in the alternating architecture, the IC.

Two pipeline architectures are supported:

- **Standard Architecture**: Processes multiple microphone channels through both AEC and IC sequentially
- **Alternating Architecture**: Selectively enables AEC or IC based on reference signal presence,
  reducing memory requirements and enabling longer AEC filter tails

Standard Architecture
---------------------

In the Standard Architecture pipeline form, all the modules are enabled and called sequentially. This is shown in
:numref:`std_arch_pipeline`.

.. _std_arch_pipeline:

.. figure:: ../images/standard_arch_pipeline.drawio.svg
    :align: center

    The Standard Architecture Pipeline.

The AEC is configured for 2 mic input channels, 2 reference input channels, 10 phase main filter and a 5 phase shadow
filter. The IC is configured for 2 mic input channels, and 10 phase main filter. The AEC generates the echo cancelled
version of the mic input that is then sent for processing through the IC. When ADEC goes into delay estimation mode,
the IC remains active.

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

    The Alternating Architecture.

When reference audio is detected, the AEC is enabled and the IC is disabled. The AEC processes one
microphone input to remove any echo from the reference signal. This is shown in :numref:`alt_arch_aec`. Note
the VNR output from the IC is still generated so that it can be used by the AGC.

.. _alt_arch_aec:

.. figure:: ../images/alt_arch_stage1_aec.drawio.svg
    :align: center

    The Alternating Architecture when the reference signal is present.

When no reference audio is detected, the AEC is disabled and the IC is enabled. The IC processes
both microphone inputs to remove any unwanted noise sources in the environment.
This is shown in :numref:`alt_arch_ic`.

.. _alt_arch_ic:

.. figure:: ../images/alt_arch_stage1_ic.drawio.svg
    :align: center

    The Alternating Architecture when no reference signal is present.

The AEC is configured for 1 mic input channel, 2 reference input channels, 15 phase main filter and a 5 phase shadow
filter giving an extended tail length for highly reverberant environments. The AEC memory pool and schedule are sized
for this configuration by setting ``AEC_SCHEDULE_CONFIG`` to ``"1 1 2 15 5"`` (see :ref:`aec-schedules`).

The switching is done inside the modules rather than by the application:

- The AEC bypasses itself while its held reference active flag is clear, so it stays enabled for
  :c:macro:`HOLD_AEC_LIMIT_SECONDS` after the reference becomes inactive. While bypassed, the microphone input
  is copied directly to the AEC output, so the output is sample aligned with the input.
- The IC bypasses itself while the reference active flag passed to :c:func:`ic_process_frame()` is set (see
  :ref:`ic_module`). Passing the flag reported by :c:func:`aec_process_frame()` ensures the AEC and IC are never
  both enabled.

The AEC is configured, and its memory sized, for 1 microphone channel only. The second microphone channel
bypasses the AEC entirely and is passed straight to the IC, alongside the AEC output. Because the bypassed AEC
output is sample aligned with its input, the IC receives both microphone channels with their original phase
relationship preserved whenever it is enabled. While the AEC is enabled, the IC is bypassed, so its output and
VNR estimate depend only on the AEC output. For example, without the :ref:`stage1_module`:

.. code-block:: c

    // AEC_MAX_Y_CHANNELS is 1, the pipeline carries 2 microphone channels
    aec_init(&aec_state, 1, 2, 15, 5, &tdist);

    int32_t aec_out[1][AEC_FRAME_ADVANCE];
    int32_t ref_active_flag;
    // Only mic[0] is processed by the AEC
    aec_process_frame(&aec_state, aec_out, NULL, mic, ref, &ref_active_flag);
    // mic[1] bypasses the AEC
    ic_process_frame(&ic_state, ic_out, aec_out[0], mic[1], &vnr_pred, ref_active_flag);

The :ref:`stage1_module` does the same, passing microphone channels the AEC is not configured for straight
through to its output.

When ADEC goes into delay estimation mode, the AEC gets reconfigured as described in the
:ref:`adec_module` documentation. This restarts the reference activity hold.

Alternating architecture is disabled by default (see :c:macro:`ALT_ARCH_MODE`). To enable it, define
``ALT_ARCH_MODE`` to 1 for the whole application, for example in the application's CMakeLists.txt. The same value is
used by the AEC and the IC.

