cmake_minimum_required(VERSION 3.22)

project(LongLand VERSION 0.1.0 LANGUAGES C CXX)

option(LONGLAND_BUILD_PLUGIN "Build the Long Land VST3" ON)
option(LONGLAND_BUILD_TESTS "Build deterministic DSP tests" ON)

add_library(LongLandDSP STATIC
    Source/LongLandDSP.cpp
    Source/LongLandDSP.h)
target_compile_features(LongLandDSP PUBLIC cxx_std_20)
target_include_directories(LongLandDSP PUBLIC Source)
if(MSVC)
    target_compile_options(LongLandDSP PRIVATE
        /W4 /permissive- /fp:fast
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Ob2>)
    target_compile_definitions(LongLandDSP PRIVATE $<$<CONFIG:Release>:NDEBUG>)
else()
    target_compile_options(LongLandDSP PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>)
    target_compile_definitions(LongLandDSP PRIVATE $<$<CONFIG:Release>:NDEBUG>)
endif()

if(LONGLAND_BUILD_TESTS)
    enable_testing()
    add_executable(LongLandDSPTests Tests/DSPTests.cpp)
    target_link_libraries(LongLandDSPTests PRIVATE LongLandDSP)
    add_test(NAME LongLandDSPTests COMMAND LongLandDSPTests)
endif()

if(LONGLAND_BUILD_PLUGIN)
    if(NOT DEFINED JUCE_DIR)
        message(FATAL_ERROR "Set JUCE_DIR to a JUCE 8 checkout, e.g. -DJUCE_DIR=C:/SDK/JUCE")
    endif()
    add_subdirectory(${JUCE_DIR} JUCE)

    juce_add_binary_data(LongLandDisplayArtwork
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/longland-3d-primary-clean.png
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/longland-3d-detail-clean.png
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/longland-red-knob-atlas.png)

    juce_add_plugin(LongLand
        COMPANY_NAME "Long Land Audio"
        BUNDLE_ID "audio.longland.reverb"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE LnLd
        PLUGIN_CODE Llrv
        FORMATS VST3 Standalone
        VST3_CATEGORIES Fx Reverb
        PRODUCT_NAME "Long Land")
    juce_generate_juce_header(LongLand)

    target_sources(LongLand PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)
    target_compile_features(LongLand PRIVATE cxx_std_20)
    target_compile_definitions(LongLand PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(LongLand PRIVATE
        LongLandDisplayArtwork
        LongLandDSP
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

    if(LONGLAND_BUILD_TESTS)
        add_executable(LongLandLifecycleTests Tests/PluginLifecycleTests.cpp)
        target_compile_features(LongLandLifecycleTests PRIVATE cxx_std_20)
        target_include_directories(LongLandLifecycleTests PRIVATE
            Source
            ${CMAKE_CURRENT_BINARY_DIR}/LongLand_artefacts/JuceLibraryCode
            ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_LongLandDisplayArtwork/JuceLibraryCode)
        target_link_libraries(LongLandLifecycleTests PRIVATE
            LongLand
            LongLandDisplayArtwork
            juce::juce_audio_utils)
        add_test(NAME LongLandLifecycleTests
            COMMAND LongLandLifecycleTests "${CMAKE_CURRENT_SOURCE_DIR}/../work/vst3-r4")

        add_executable(LongLandVST3Probe Tests/VST3Probe.cpp)
        target_compile_features(LongLandVST3Probe PRIVATE cxx_std_20)
        target_compile_definitions(LongLandVST3Probe PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_PLUGINHOST_VST3=1
            JUCE_VST3_CAN_REPLACE_VST2=0)
        target_link_libraries(LongLandVST3Probe PRIVATE
            juce::juce_audio_utils
            juce::juce_recommended_config_flags)
        add_dependencies(LongLandVST3Probe LongLand_VST3)
        add_test(NAME LongLandVST3Probe
            COMMAND LongLandVST3Probe
                "${CMAKE_CURRENT_BINARY_DIR}/LongLand_artefacts/$<CONFIG>/VST3/Long Land.vst3")
    endif()
endif()
