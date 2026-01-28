Noise Suppressor
=================

The NS provides an API to implement the Noise
Suppressor within an application.

The noise suppressor estimates the probability of speech presence and dynamically
adapts its coefficients to estimate the noise levels to subtract from the input.
The filter will automatically reset its noise estimations 
every :c:member:`ns_state_t.reset_period`, which is 10 frames by default.

The NS takes as input a frame of data from an audio channel. This could be the
microphone input or the output of another module in the application.

Noise suppression is performed on a frame-by-frame basis. Each frame consists of
15ms of data, which is 240 samples at 16kHz input sampling frequency. Input data is
expected to be in a fixed-point 32-bit 1.31 format.

Before processing any frames, the application must configure and initialise the
NS instance by calling :c:func:`ns_init()`. Then for each frame,
:c:func:`ns_process_frame()` will update the NS instance's internal state and produce
the output frame by applying the NS algorithm to the input frame.
Refer to the :ref:`pipeline_example` to see how the APIs above are used.

If multiple channels need to be processed by the application, or multiple outputs
are required, an independent instance of the NS must be run for each channel.
