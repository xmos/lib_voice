|newpage|

.. _examples_section:

********
Examples
********

This section goes through the various example applications that are provided alongside the ``lib_voice``.
The examples are meant to demonstrate the basic usage of the APIs.
All examples are located in the ``examples`` directory.

.. _aec_example:

AEC example
===========

The AEC example demonstrates the use of API to run a frame of data through the AEC.
It also shows how to configure AEC to run on one or two threads.
``AEC_THREADS`` define toggles the AEC to use 1-threaded or 2-threaded scheduling structs:

.. literalinclude:: ../../../examples/app_aec/src/main.c
    :language: c
    :start-at: #if AEC_THREADS == 1
    :end-before: // Allocate signal data

The build system will automatically create both configs called ``app_aec_1th`` and ``app_aec_2th``.

After the application has decided which thread distribution scheme to run,
it allocates memory for the AEC and initialises it:

.. literalinclude:: ../../../examples/app_aec/src/main.c
    :language: c
    :start-at: // Allocate signal data
    :end-at: AEC_MAIN_FILTER_PHASES, AEC_SHADOW_FILTER_PHASES, &tdist);

After the AEC has been initialised, it is ready to process data.
In this example, ``frame_y`` and ``frame_x`` memory is reused for AEC output:

.. literalinclude:: ../../../examples/app_aec/src/main.c
    :language: c
    :start-at: producer(frame_y, frame_x);
    :end-at: consumer(frame_y);

Upon execution, the example will print "frame done" when the AEC has processed a frame.

.. _vnr_example:

VNR example
===========

The VNR example demonstrates the use of API to run a frame of data through the VNR
and get the estimation of how much voice is present in it.

To run the VNR, the application should first allocate memory for its data and initialise it:

.. literalinclude:: ../../../examples/app_vnr/src/main.c
    :language: c
    :start-at: // Allocate input and output memory
    :end-at: vnr_state_init(&vnr);

After the VNR has been initialised, it is ready to process data.
The VNR output is in ``float_s32_t`` format, so there's an extra step if the user wants it in ``float``.

.. literalinclude:: ../../../examples/app_vnr/src/main.c
    :language: c
    :start-at: producer(input);
    :end-at: consumer(res);

Upon execution, the example will use the pseudo-random generator to get an input data.
This data will be run through the VNR and the score will be printed in the terminal.
It outputs a number between 0 and 1, 1 being the strongest voice with respect to noise
and 0 being the lowest voice compared to noise ratio.
The pseudo-random data is not representative of a real signal,
so the VNR scores in this example tend to be zero.

.. _pipeline_example:

Pipeline example
================

This example demonstrates how to put together a voice processing pipeline.
The pipeline will have AEC, IC, NS and AGC stages. It also demonstrates the use of ADEC module to
do a one time estimation and correction for possible reference and loudspeaker delay offsets at start up in order to
maximise AEC performance.  ADEC processing happens on the same thread as the AEC. The VNR is introduced
to give the IC and the AGC information about the speech presence in a frame.

There are two pipelines supported in this example: Standard Architecture and Alternating Architecture.
Building one or the other config is controlled via the ``ALT_ARCH_MODE`` define.
The build system will automatically create both configs called ``app_pipeline_std_arch`` and ``app_pipeline_alt_arch``.

To create the pipeline, the application must first initialise all the individual components:

.. literalinclude:: ../../../examples/app_pipeline/src/pipeline.c
    :language: c
    :start-at: // Initialise AEC, DE, ADEC stages
    :end-at: stage1_init(&state->stage_1_state, &aec_de_mode_conf, &aec_non_de_mode_conf, &adec_conf);

.. literalinclude:: ../../../examples/app_pipeline/src/pipeline.c
    :language: c
    :start-at: // Initialise IC, VNR
    :end-at: agc_init(&state->agc_state, &agc_conf_asr);

After the pipeline has been initialised, the data can be run through it.
All the modules in the example can run on separate threads.
To exchange information between the pipeline stages
the metadata struct will need to be created to be populated and consumed by the different modules.

.. literalinclude:: ../../../examples/app_pipeline/src/pipeline.c
    :language: c
    :start-at: /** Stage1 - AEC, DE, ADEC*/
    :end-at: &md.aec_corr_factor, &md.ref_active_flag, input_y_data, input_x_data);

.. literalinclude:: ../../../examples/app_pipeline/src/pipeline.c
    :language: c
    :start-at: // Bypass IC if the reference is high in the alt arch mode
    :end-at: agc_process_frame(&state->agc_state, output_data, ns_output, &agc_md);

Upon execution, the example will print "frame done" when the AEC has processed a frame.

Building the example
====================

This section assumes that the `XMOS XTC Tools <https://www.xmos.com/software-tools/>`_ have been downloaded and installed.
It also assumes that the Python is installed and available.
The required versions are specified in the accompanying ``README``.

Installation instructions can be found `here <https://xmos.com/xtc-install-guide>`_.

Special attention should be paid to the section on
`Installation of Required Third-Party Tools <https://www.xmos.com/documentation/XM-014363-PC/html/installation/install-configure/install-tools/install_prerequisites.html>`_.

The application is built using the `xcommon-cmake <https://www.xmos.com/file/xcommon-cmake-documentation/?version=latest>`_
build system, which is provided with the XTC tools and is based on `CMake <https://cmake.org/>`_.

The ``lib_voice`` software ZIP package should be downloaded and extracted to a chosen working
directory.

To configure the build, the following commands should be run from an XTC command prompt:

.. code-block:: bash

    cd lib_voice
    pip install -r requirements.txt
    cd examples/app_aec
    cmake -G "Unix Makefiles" -B build

.. note::

    The ``pip install -r requirements.txt`` stage has to happen before the ``cmake`` configuration
    as it will fetch the `xmos-ai-tools <https://pypi.org/project/xmos-ai-tools/>`_.

If any dependencies are missing they will be retrieved automatically during this step.

The application binaries should then be built using ``xmake``:

.. code-block:: bash

    xmake -j -C build

Binary artifacts (.xe files) will be generated under the appropriate subdirectories of the
``app_aec/bin`` directory — one for each supported build configuration.

For subsequent builds, the ``cmake`` step may be omitted.
If ``CMakeLists.txt`` or other build files are modified, ``cmake`` will be re-run automatically
by ``xmake`` as needed.

Running the example
===================

From an XTC command prompt, the following command should be run from the ``examples/app_aec``
directory:

.. code-block:: bash

    xrun --io ./bin/2th/app_aec_2th.xe

