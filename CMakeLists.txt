cmake_minimum_required(VERSION 3.22)
project(ANKS_Swarm_01 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set -DANKS_JUCE_PATH=/path/to/juce to use an existing JUCE checkout.
# Otherwise the locked official JUCE 8 security release is downloaded once.
if (DEFINED ANKS_JUCE_PATH AND EXISTS "${ANKS_JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${ANKS_JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce")
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.17-security
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(ANKS_Swarm_01
    COMPANY_NAME "ANKS Labs"
    BUNDLE_ID "labs.anks.swarm01"
    PLUGIN_MANUFACTURER_CODE AnkS
    PLUGIN_CODE Sw01
    PRODUCT_NAME "ANKS SWARM 01"
    DESCRIPTION "Dual swarm synthesizer with four-corner morphing"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(ANKS_Swarm_01)

target_sources(ANKS_Swarm_01 PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/DSP/SwarmEngine.h
)

target_compile_definitions(ANKS_Swarm_01 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ANKS_Swarm_01 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags
)

# Standalone test is independent of JUCE and may also be compiled with g++.
option(ANKS_BUILD_DSP_TESTS "Build headless DSP smoke tests" ON)
if (ANKS_BUILD_DSP_TESTS)
    enable_testing()
    add_executable(swarm_dsp_test Tests/test_dsp.cpp Source/DSP/SwarmEngine.h)
    target_compile_features(swarm_dsp_test PRIVATE cxx_std_17)
    add_test(NAME SwarmDSP COMMAND swarm_dsp_test)
    add_executable(swarm_dsp_stress Tests/stress_dsp.cpp Source/DSP/SwarmEngine.h)
    target_compile_features(swarm_dsp_stress PRIVATE cxx_std_17)
    add_test(NAME SwarmStress COMMAND swarm_dsp_stress)
endif()
