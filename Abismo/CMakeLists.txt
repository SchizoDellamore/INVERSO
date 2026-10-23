cmake_minimum_required(VERSION 3.22)
project(InversoAbismo VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(INVERSO_DSP_TESTS_ONLY "Build portable DSP tests without JUCE" OFF)
enable_testing()
add_executable(abismo_engine Tests/test_engine.cpp)
add_executable(abismo_presets Tests/test_presets.cpp)
add_executable(abismo_stress Tests/test_stress.cpp)
foreach(t IN ITEMS abismo_engine abismo_presets abismo_stress)
 target_compile_features(${t} PRIVATE cxx_std_17)
 if(MSVC)
  target_compile_options(${t} PRIVATE /W4)
 else()
  target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
 endif()
endforeach()
add_test(NAME abismo_engine COMMAND abismo_engine)
add_test(NAME abismo_presets COMMAND abismo_presets)
add_test(NAME abismo_stress COMMAND abismo_stress)
if(INVERSO_DSP_TESTS_ONLY)
 return()
endif()
if(DEFINED JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
 add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
 find_package(JUCE CONFIG REQUIRED)
endif()
set(ABISMO_SOURCES
 Source/PluginProcessor.cpp Source/PluginProcessor.h
 Source/PluginEditor.cpp Source/PluginEditor.h
 Source/DSP/AbismoParams.h Source/DSP/AbismoEngine.h Source/DSP/AbismoReverb.h Source/DSP/AbismoMutilator.h)
juce_add_plugin(InversoAbismo
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.abismo"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvAb
 PRODUCT_NAME "INVERSO ABISMO"
 FORMATS VST3
 VST3_CATEGORIES Instrument Synth
 IS_SYNTH TRUE
 NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoAbismo)
target_sources(InversoAbismo PRIVATE ${ABISMO_SOURCES})
target_compile_definitions(InversoAbismo PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismo PRIVATE
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 juce::juce_recommended_lto_flags)

# Drives the JUCE processor like a DAW: MIDI at varying block sizes, programs,
# automation and state save/restore. Builds on the Windows runner.
juce_add_console_app(InversoAbismoHostSmoke PRODUCT_NAME "INVERSO Abismo Host Smoke")
juce_generate_juce_header(InversoAbismoHostSmoke)
target_sources(InversoAbismoHostSmoke PRIVATE Tests/test_host.cpp ${ABISMO_SOURCES})
target_compile_definitions(InversoAbismoHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoHostSmoke PRIVATE
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
add_test(NAME abismo_host_smoke COMMAND InversoAbismoHostSmoke)
