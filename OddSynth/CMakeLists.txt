cmake_minimum_required(VERSION 3.24)
project(InversoOddSynth VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
add_executable(OddSynthCoreTests Tests/test_core.cpp)
add_test(NAME oddsynth_control_and_audio COMMAND OddSynthCoreTests)
option(ODDSYNTH_CORE_ONLY "Build portable DSP and control tests only" OFF)
if(ODDSYNTH_CORE_ONLY)
  return()
endif()
if(NOT DEFINED JUCE_ROOT)
  message(FATAL_ERROR "Set JUCE_ROOT to the pinned JUCE checkout")
endif()
add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
juce_add_plugin(InversoOddSynth
  COMPANY_NAME "INVERSO STUDIOS"
  BUNDLE_ID "net.inversostudios.oddsynth"
  PLUGIN_MANUFACTURER_CODE InvS
  PLUGIN_CODE IvOs
  PRODUCT_NAME "INVERSO OddSynth"
  FORMATS VST3 Standalone
  VST3_CATEGORIES Instrument Synth Sampler
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE)
set(ODD_SOURCES Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h Source/OddCore.h)
juce_generate_juce_header(InversoOddSynth)
target_sources(InversoOddSynth PRIVATE ${ODD_SOURCES})
target_compile_definitions(InversoOddSynth PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoOddSynth PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
juce_add_console_app(OddSynthHostTests PRODUCT_NAME "OddSynth Host Tests")
juce_generate_juce_header(OddSynthHostTests)
target_sources(OddSynthHostTests PRIVATE Tests/test_host.cpp ${ODD_SOURCES})
target_compile_definitions(OddSynthHostTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(OddSynthHostTests PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
  target_compile_options(InversoOddSynth PRIVATE /utf-8)
  target_compile_options(OddSynthHostTests PRIVATE /utf-8)
endif()
add_dependencies(OddSynthHostTests InversoOddSynth_VST3)
add_test(NAME oddsynth_native_host COMMAND OddSynthHostTests "${CMAKE_CURRENT_BINARY_DIR}/InversoOddSynth_artefacts/$<CONFIG>/VST3/INVERSO OddSynth.vst3" "${CMAKE_CURRENT_BINARY_DIR}/test-output")
