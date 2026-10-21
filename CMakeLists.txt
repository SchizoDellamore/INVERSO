cmake_minimum_required(VERSION 3.22)
project(INVERSO_EQ VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Portable DSP tests can be built on Linux without a JUCE dependency.
option(INVERSO_DSP_TESTS_ONLY "Build portable DSP tests only (no JUCE required)" OFF)
enable_testing()
add_executable(inverso_eq_test Tests/test_eq.cpp)
add_executable(inverso_eq_stress Tests/test_stress.cpp)
add_executable(inverso_eq_automation Tests/test_automation.cpp)
add_executable(inverso_eq_truepeak Tests/test_truepeak.cpp)
add_executable(inverso_eq_instances Tests/test_instances.cpp)
foreach(test_target IN ITEMS inverso_eq_test inverso_eq_stress inverso_eq_automation inverso_eq_truepeak inverso_eq_instances)
    target_compile_features(${test_target} PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /W4)
    else()
        target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_test(NAME eq_functional COMMAND inverso_eq_test)
add_test(NAME eq_stress COMMAND inverso_eq_stress)
add_test(NAME eq_automation COMMAND inverso_eq_automation)
add_test(NAME eq_truepeak COMMAND inverso_eq_truepeak)
add_test(NAME eq_instances COMMAND inverso_eq_instances)
if(INVERSO_DSP_TESTS_ONLY)
    return()
endif()

# Windows plugin build requires JUCE. It is fetched separately by BUILD_AND_PACKAGE.ps1.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(DEFINED JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
    add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_plugin(InversoEQ
    COMPANY_NAME "Inverso Studios"
    BUNDLE_ID "net.inversostudios.inversoeq"
    PLUGIN_MANUFACTURER_CODE InvS
    PLUGIN_CODE IvEQ
    PRODUCT_NAME "INVERSO EQ"
    FORMATS VST3
    VST3_CATEGORIES EQ
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(InversoEQ)

target_sources(InversoEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/EQCore.h
)

target_compile_definitions(InversoEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(InversoEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
)
