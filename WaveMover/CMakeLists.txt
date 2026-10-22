cmake_minimum_required(VERSION 3.24)
project(InversoWaveMover VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
if(NOT DEFINED STRETCH_ROOT OR NOT DEFINED LINEAR_ROOT)
  message(FATAL_ERROR "Set STRETCH_ROOT and LINEAR_ROOT to the pinned Signalsmith source directories")
endif()
add_executable(WaveMoverDSPTests Tests/test_edit.cpp)
target_include_directories(WaveMoverDSPTests PRIVATE "${STRETCH_ROOT}" "${LINEAR_ROOT}/include")
add_test(NAME wavemover_audio_edits COMMAND WaveMoverDSPTests)
option(WAVEMOVER_DSP_ONLY "Only build the portable audio tests" OFF)
if(WAVEMOVER_DSP_ONLY)
  return()
endif()
if(NOT DEFINED JUCE_ROOT)
  message(FATAL_ERROR "Set JUCE_ROOT to JUCE 8.0.16-security")
endif()
add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
juce_add_plugin(InversoWaveMover
  COMPANY_NAME "Inverso Studios"
  BUNDLE_ID "net.inversostudios.wavemover"
  PLUGIN_MANUFACTURER_CODE InvS
  PLUGIN_CODE IvWm
  PRODUCT_NAME "INVERSO WaveMover"
  FORMATS VST3 Standalone
  VST3_CATEGORIES Fx Tools
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE)
set(WAVEMOVER_SOURCES Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h Source/AudioEdit.h)
juce_generate_juce_header(InversoWaveMover)
target_sources(InversoWaveMover PRIVATE ${WAVEMOVER_SOURCES})
target_include_directories(InversoWaveMover PRIVATE "${STRETCH_ROOT}" "${LINEAR_ROOT}/include")
target_compile_definitions(InversoWaveMover PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoWaveMover PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
juce_add_console_app(WaveMoverHostTests PRODUCT_NAME "WaveMover Host Tests")
juce_generate_juce_header(WaveMoverHostTests)
target_sources(WaveMoverHostTests PRIVATE Tests/test_host.cpp ${WAVEMOVER_SOURCES})
target_include_directories(WaveMoverHostTests PRIVATE "${STRETCH_ROOT}" "${LINEAR_ROOT}/include")
target_compile_definitions(WaveMoverHostTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(WaveMoverHostTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_dependencies(WaveMoverHostTests InversoWaveMover_VST3)
add_test(NAME wavemover_native_host COMMAND WaveMoverHostTests "${CMAKE_CURRENT_BINARY_DIR}/InversoWaveMover_artefacts/$<CONFIG>/VST3/INVERSO WaveMover.vst3" "${CMAKE_CURRENT_BINARY_DIR}/test-output")
