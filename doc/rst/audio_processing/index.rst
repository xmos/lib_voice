|newpage|

.. _audio_processing_section:

****************
Audio processing
****************

This section describes the functionality of each ``lib_voice`` modules, diving into the details of the underlying DSP.

All the modules have been designed to be used together in a voice processing pipeline, but they can
also be used independently or in custom combinations. The modules have been designed to work with a
16 kHz sample rates, but may work at other samples rates if the time domain parameters are adjusted
correctly.

Many of the modules in ``lib_voice`` use block floating point (BFP) arithmetic to achieve high
performance on fixed-point hardware. In BFP, a block of data shares a common exponent, allowing for
efficient scaling and dynamic range management. For more information on BFP, see the `XCore BFP
documentation <https://www.xmos.com/documentation/XM-015059-UG/html/doc/rst/src/bfp_background.html>`_.

.. toctree::
  :maxdepth: 1

  aec
  ic
  vnr
  ns
  agc
  adec
  stage1
  alt_arch