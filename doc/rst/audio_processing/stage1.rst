.. _stage1_module:

Pipeline Stage 1
================

Stage 1 stage 1

Standard Architecture
---------------------

In this pipeline form, all the modules are enabled and called sequentially.

The AEC is configured for 2 mic input channels, 2 reference input channels, 10 phase main filter and a 5 phase shadow
filter. The AEC gets reconfigured as a 1 mic input channel, 1 reference input channel, 30 main filter phases and no shadow
filter, when ADEC goes in delay estimation mode. This allows it to measure the room delay. During this process, the AEC
output is ignored and the mic input is directly sent to output. Once the new delay has been measured and the delay correction is
applied, the AEC gets configured back to its original configuration and starts adapting and cancellation.
The AEC stage generates the echo cancelled version of the mic input that is then sent for processing through the IC.

Alternating Architecture
------------------------

In this pipeline form, the AEC and the IC frame processing are selectively enabled and disabled based on the presence of reference input signal.
Acoustic Echo Cancellation is performed only if activity is detected on the reference input channels and disabled otherwise.
Interference Cancellation is performed only when AEC is disabled so in the absence of reference channel activity and disabled otherwise.

The AEC is configured for 1 mic input channel, 2 reference input channels, 15 phase main filter and a 5 phase shadow
filter giving an extended tail length for highly reverberant environments. The AEC gets reconfigured as a 1 mic input channel, 1 reference input channel, 30 main filter phases and no shadow
filter, when ADEC goes in delay estimation mode. This allows it to measure the room delay. During this process, the AEC
output is ignored and the mic input is directly sent to output. Once the new delay has been measured and the delay correction is
applied, the AEC gets configured back to its original configuration and starts adapting and cancellation.
In the absence of activity on the reference channels, when the AEC is disabled, the mic input is copied directly to the output of the AEC.

