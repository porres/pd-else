###############################
# MEC library
project(MILib)

set(BRAIDS_SRC
    braids/braids_resources.cc
    braids/analog_oscillator.cc
    braids/digital_oscillator.cc
    braids/macro_oscillator.cc
    )

set(WARPS_SRC
       warps/warps_resources.cc
       warps/dsp/filter_bank.cc
       warps/dsp/modulator.cc
       warps/dsp/oscillator.cc
       warps/dsp/vocoder.cc
        )

set(ELEMENTS_SRC
       elements/elements_resources.cc
       elements/dsp/multistage_envelope.cc
       elements/dsp/ominous_voice.cc
       elements/dsp/resonator.cc
       elements/dsp/string.cc
       elements/dsp/tube.cc
       elements/dsp/exciter.cc
       elements/dsp/part.cc
       elements/dsp/voice.cc
       ) 

set(CLOUDS_SRC
       clouds/clouds_resources.cc
       clouds/dsp/correlator.cc
       clouds/dsp/mu_law.cc
       clouds/dsp/granular_processor.cc
       clouds/dsp/pvoc/frame_transformation.cc
       clouds/dsp/pvoc/phase_vocoder.cc
       clouds/dsp/pvoc/stft.cc 
       )

set(RINGS_SRC
       rings/rings_resources.cc 
       rings/dsp/fm_voice.cc 
       rings/dsp/resonator.cc 
       rings/dsp/part.cc 
       rings/dsp/string.cc 
       rings/dsp/string_synth_part.cc
       )

set(STREAMS_SRC
       streams/streams_resources.cc
       streams/compressor.cc
       streams/envelope.cc
       streams/follower.cc
       streams/lorenz_generator.cc
       streams/processor.cc
       streams/svf.cc
       streams/vactrol.cc
       )

set(MI_COMMON_SRC
       stmlib/dsp/units.cc
       stmlib/dsp/atan.cc 
       stmlib/utils/random.cc
)

set(MILIB_SRC 
        ${MI_COMMON_SRC}
        ${STREAMS_SRC}
        ${CLOUDS_SRC}
        ${ELMENTS_SRC}
        ${WARPS_SRC}
        ${RINGS_SRC}
        ${BRAIDS_SRC}
        ${ELEMENTS_SRC}
        )

include_directories(
        "${PROJECT_SOURCE_DIR}"
)


add_library(MILib STATIC ${MILIB_SRC})

