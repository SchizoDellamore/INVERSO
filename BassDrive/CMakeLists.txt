cmake_minimum_required(VERSION 3.22)
project(InversoBassDrive VERSION 0.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(INVERSO_DSP_TESTS_ONLY "Build portable DSP tests without JUCE" OFF)
enable_testing()
add_executable(bassdrive_functional Tests/test_bassdrive.cpp)
add_executable(bassdrive_stress Tests/test_stress.cpp)
add_executable(bassdrive_passthrough Tests/test_passthrough.cpp)
add_executable(bassdrive_color Tests/test_color.cpp)
foreach(t IN ITEMS bassdrive_functional bassdrive_stress bassdrive_passthrough bassdrive_color)
 target_compile_features(${t} PRIVATE cxx_std_17)
 if(MSVC)
  target_compile_options(${t} PRIVATE /W4)
 else()
  target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
 endif()
endforeach()
add_test(NAME bassdrive_functional COMMAND bassdrive_functional)
add_test(NAME bassdrive_stress COMMAND bassdrive_stress)
add_test(NAME bassdrive_passthrough COMMAND bassdrive_passthrough)
add_test(NAME bassdrive_color COMMAND bassdrive_color)
if(INVERSO_DSP_TESTS_ONLY)
 return()
endif()
if(DEFINED JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
 add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
 find_package(JUCE CONFIG REQUIRED)
endif()
juce_add_plugin(InversoBassDrive
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.bassdrive"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvBD
 PRODUCT_NAME "INVERSO BASS DRIVE"
 FORMATS VST3
 VST3_CATEGORIES Fx
 IS_SYNTH FALSE
 NEEDS_MIDI_INPUT FALSE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoBassDrive)
target_sources(InversoBassDrive PRIVATE
 Source/PluginProcessor.cpp Source/PluginProcessor.h
 Source/PluginEditor.cpp Source/PluginEditor.h
 Source/DSP/BassDriveCore.h)
target_compile_definitions(InversoBassDrive PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoBassDrive PRIVATE
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 juce::juce_recommended_lto_flags)

# Exercise the JUCE processor in a DAW-like callback (mono/stereo, varying
# host block sizes, automation, state restore). Builds on the Windows runner.
juce_add_console_app(InversoBassDriveHostSmoke PRODUCT_NAME "INVERSO Bass Drive Host Smoke")
juce_generate_juce_header(InversoBassDriveHostSmoke)
target_sources(InversoBassDriveHostSmoke PRIVATE
 Tests/test_host.cpp Source/PluginProcessor.cpp Source/PluginProcessor.h
 Source/PluginEditor.cpp Source/PluginEditor.h Source/DSP/BassDriveCore.h)
target_compile_definitions(InversoBassDriveHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoBassDriveHostSmoke PRIVATE
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
add_test(NAME bassdrive_host_smoke COMMAND InversoBassDriveHostSmoke)
