cmake_minimum_required(VERSION 3.22)
project(ANKS_Swarm_01 VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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

if (MSVC)
    target_compile_options(ANKS_Swarm_01 PRIVATE /utf-8)
endif()

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

target_sources(ANKS_Swarm_01_Standalone PRIVATE Source/PortableStandalone.cpp)
target_compile_definitions(ANKS_Swarm_01_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_include_directories(ANKS_Swarm_01_Standalone PRIVATE
    "$<TARGET_PROPERTY:ANKS_Swarm_01,JUCE_GENERATED_SOURCES_DIRECTORY>")

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
    add_executable(swarm_controls_test Tests/test_controls.cpp)
    target_link_libraries(swarm_controls_test PRIVATE ANKS_Swarm_01)
    target_include_directories(swarm_controls_test PRIVATE
        "$<TARGET_PROPERTY:ANKS_Swarm_01,INCLUDE_DIRECTORIES>")
    add_test(NAME SwarmControls COMMAND swarm_controls_test)

endif()

if (WIN32)
    add_custom_target(swarm_windows_package
        COMMAND "${CMAKE_COMMAND}"
            "-DPLUGIN_BINARY=$<TARGET_FILE:ANKS_Swarm_01_VST3>"
            "-DSTANDALONE=$<TARGET_FILE:ANKS_Swarm_01_Standalone>"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/packages/$<CONFIG>"
            "-DVERSION=${PROJECT_VERSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageWindows.cmake"
        DEPENDS ANKS_Swarm_01_VST3 ANKS_Swarm_01_Standalone
        VERBATIM)
endif()
