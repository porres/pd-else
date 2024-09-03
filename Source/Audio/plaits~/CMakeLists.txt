cmake_minimum_required(VERSION 3.10)

# Project name and version
project(plaits_tilde VERSION 1.0)

# Add the pd-cmake module
include(${CMAKE_SOURCE_DIR}/pd.cmake)

# Set the library name
set(LIB_NAME "plaits_tilde")

# Add the source files
set(COMMON_SOURCES
    stmlib/dsp/units.cc
    stmlib/utils/random.cc
    stmlib/dsp/atan.cc
    plaits/dsp/voice.cc
    plaits/dsp/engine/additive_engine.cc
    plaits/dsp/engine/bass_drum_engine.cc
    plaits/dsp/engine/chord_engine.cc
    plaits/dsp/engine/fm_engine.cc
    plaits/dsp/engine/grain_engine.cc
    plaits/dsp/engine/hi_hat_engine.cc
    plaits/dsp/engine/modal_engine.cc
    plaits/dsp/engine/noise_engine.cc
    plaits/dsp/engine/particle_engine.cc
    plaits/dsp/engine/snare_drum_engine.cc
    plaits/dsp/engine/speech_engine.cc
    plaits/dsp/engine/string_engine.cc
    plaits/dsp/engine/swarm_engine.cc
    plaits/dsp/engine/virtual_analog_engine.cc
    plaits/dsp/engine/waveshaping_engine.cc
    plaits/dsp/engine/wavetable_engine.cc
    plaits/dsp/speech/lpc_speech_synth.cc
    plaits/dsp/speech/lpc_speech_synth_controller.cc
    plaits/dsp/speech/lpc_speech_synth_phonemes.cc
    plaits/dsp/speech/lpc_speech_synth_words.cc
    plaits/dsp/speech/naive_speech_synth.cc
    plaits/dsp/speech/sam_speech_synth.cc
    plaits/dsp/physical_modelling/modal_voice.cc
    plaits/dsp/physical_modelling/resonator.cc
    plaits/dsp/physical_modelling/string.cc
    plaits/dsp/physical_modelling/string_voice.cc
    plaits/dsp/engine2/chiptune_engine.cc
    plaits/dsp/engine2/phase_distortion_engine.cc
    plaits/dsp/engine2/six_op_engine.cc
    plaits/dsp/engine2/string_machine_engine.cc
    plaits/dsp/engine2/virtual_analog_vcf_engine.cc
    plaits/dsp/engine2/wave_terrain_engine.cc
    plaits/dsp/fm/algorithms.cc
    plaits/dsp/fm/dx_units.cc
    plaits/dsp/chords/chord_bank.cc
    plaits/resources.cc
)

set(CLASS_SOURCES
    plaits~.cpp
)

# Combine all sources
set(SOURCES ${COMMON_SOURCES} ${CLASS_SOURCES})

# Create the Pure Data external
add_pd_external(${LIB_NAME}_obj ${LIB_NAME} ${SOURCES})

# Set compiler flags
target_compile_options(${LIB_NAME}_obj PRIVATE
    -DTEST
    -Wno-unused-local-typedefs
    -I${CMAKE_CURRENT_SOURCE_DIR}
)
