cmake_minimum_required(VERSION 3.22)
project(InversoProcessors VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(INVERSO_DSP_TESTS_ONLY "DSP tests only - no JUCE" OFF)
enable_testing()
add_executable(processor_functional Tests/test_processors.cpp)
add_executable(processor_stress Tests/test_stress.cpp)
foreach(t IN ITEMS processor_functional processor_stress)
 target_compile_features(${t} PRIVATE cxx_std_17)
 if(MSVC)
  target_compile_options(${t} PRIVATE /W4)
 else()
  target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
 endif()
endforeach()
add_test(NAME processors_functional COMMAND processor_functional)
add_test(NAME processors_stress COMMAND processor_stress)
if(INVERSO_DSP_TESTS_ONLY)
 return()
endif()
if(DEFINED JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
 add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
 find_package(JUCE CONFIG REQUIRED)
endif()

function(add_inverso_processor TARGET LABEL CODE ID MUD)
 juce_add_plugin(${TARGET}
  COMPANY_NAME "Inverso Studios"
  BUNDLE_ID "net.inversostudios.${ID}"
  PLUGIN_MANUFACTURER_CODE InvS
  PLUGIN_CODE ${CODE}
  PRODUCT_NAME "${LABEL}"
  FORMATS VST3
  VST3_CATEGORIES Fx
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
 )
 juce_generate_juce_header(${TARGET})
 target_sources(${TARGET} PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h Source/DSP/AdaptiveSuppressor.h)
 target_compile_definitions(${TARGET} PRIVATE INVERSO_DEMUDDER=${MUD} JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
 target_link_libraries(${TARGET} PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
endfunction()
add_inverso_processor(InversoDeMudder "INVERSO DE-MUDDER" IvMd demudder 1)
add_inverso_processor(InversoDeAnnoyance "INVERSO DE-ANNOYANCE" IvAn deannoyance 0)
