cmake_minimum_required(VERSION 3.22)
project(InversoBerimbass VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(INVERSO_DSP_TESTS_ONLY "Build portable DSP tests without JUCE" OFF)
# Shares DSP blocks and the UI look with the ABISMO instruments.
set(ABISMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Abismo/Source")
enable_testing()
add_executable(berimbass_engine Tests/test_berimbass.cpp)
target_compile_features(berimbass_engine PRIVATE cxx_std_17)
if(MSVC)
 target_compile_options(berimbass_engine PRIVATE /W4)
else()
 target_compile_options(berimbass_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME berimbass_engine COMMAND berimbass_engine)
if(INVERSO_DSP_TESTS_ONLY)
 return()
endif()
if(DEFINED JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
 add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
 find_package(JUCE CONFIG REQUIRED)
endif()
set(BERIMBASS_SOURCES
 Source/PluginProcessor.cpp Source/PluginProcessor.h
 Source/PluginEditor.cpp Source/PluginEditor.h
 Source/DSP/BerimbassParams.h Source/DSP/BerimbassEngine.h
 ${ABISMO_DIR}/AbismoUI.cpp ${ABISMO_DIR}/AbismoUI.h
 ${ABISMO_DIR}/DSP/AbismoCommon.h ${ABISMO_DIR}/DSP/AbismoParams.h ${ABISMO_DIR}/DSP/AbismoReverb.h)
juce_add_plugin(InversoBerimbass
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.berimbass"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvBB
 PRODUCT_NAME "INVERSO BERIMBASS"
 FORMATS VST3
 VST3_CATEGORIES Instrument Synth
 IS_SYNTH TRUE
 NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoBerimbass)
target_sources(InversoBerimbass PRIVATE ${BERIMBASS_SOURCES})
target_compile_definitions(InversoBerimbass PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoBerimbass PRIVATE
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 juce::juce_recommended_lto_flags)

juce_add_console_app(InversoBerimbassHostSmoke PRODUCT_NAME "INVERSO Berimbass Host Smoke")
juce_generate_juce_header(InversoBerimbassHostSmoke)
target_sources(InversoBerimbassHostSmoke PRIVATE Tests/test_host.cpp ${BERIMBASS_SOURCES})
target_compile_definitions(InversoBerimbassHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoBerimbassHostSmoke PRIVATE
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
add_test(NAME berimbass_host_smoke COMMAND InversoBerimbassHostSmoke)
