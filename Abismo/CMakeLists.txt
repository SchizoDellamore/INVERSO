cmake_minimum_required(VERSION 3.22)
project(InversoAbismo VERSION 0.8.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(INVERSO_DSP_TESTS_ONLY "Build portable DSP tests without JUCE" OFF)
enable_testing()
add_executable(abismo_engine Tests/test_engine.cpp)
add_executable(abismo_presets Tests/test_presets.cpp)
add_executable(abismo_stress Tests/test_stress.cpp)
add_executable(abismo_sampler Tests/test_sampler.cpp)
add_executable(abismo_drums Tests/test_drums.cpp)
foreach(t IN ITEMS abismo_engine abismo_presets abismo_stress abismo_sampler abismo_drums)
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
add_test(NAME abismo_sampler COMMAND abismo_sampler)
add_test(NAME abismo_drums COMMAND abismo_drums)
target_compile_definitions(abismo_drums PRIVATE ABISMO_DRUMS_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin")
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
 Source/AbismoUI.cpp Source/AbismoUI.h
 Source/DSP/AbismoCommon.h Source/DSP/AbismoParams.h Source/DSP/AbismoEngine.h Source/DSP/AbismoReverb.h Source/DSP/AbismoMutilator.h)
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

# ABISMO SAMPLER: second instrument built from the same DSP and UI blocks.
set(SAMPLER_SOURCES
 Source/SamplerProcessor.cpp Source/SamplerProcessor.h
 Source/SamplerEditor.cpp Source/SamplerEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h
 Source/DSP/AbismoCommon.h Source/DSP/AbismoParams.h Source/DSP/AbismoEngine.h Source/DSP/AbismoReverb.h
 Source/DSP/AbismoMutilator.h Source/DSP/SamplerParams.h Source/DSP/SamplerEngine.h Source/DSP/SamplerDefault.h)
juce_add_plugin(InversoAbismoSampler
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.abismosampler"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvAS
 PRODUCT_NAME "INVERSO ABISMO SAMPLER"
 FORMATS VST3
 VST3_CATEGORIES Instrument Sampler
 IS_SYNTH TRUE
 NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoAbismoSampler)
target_sources(InversoAbismoSampler PRIVATE ${SAMPLER_SOURCES})
target_compile_definitions(InversoAbismoSampler PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoSampler PRIVATE
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 juce::juce_recommended_lto_flags)

juce_add_console_app(InversoAbismoSamplerHostSmoke PRODUCT_NAME "INVERSO Abismo Sampler Host Smoke")
juce_generate_juce_header(InversoAbismoSamplerHostSmoke)
target_sources(InversoAbismoSamplerHostSmoke PRIVATE Tests/test_sampler_host.cpp ${SAMPLER_SOURCES})
target_compile_definitions(InversoAbismoSamplerHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoSamplerHostSmoke PRIVATE
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
add_test(NAME abismo_sampler_host_smoke COMMAND InversoAbismoSamplerHostSmoke)

# ABISMO DRUMS: synthesised junkyard percussion and musical saw.
set(DRUMS_SOURCES
 Source/DrumsProcessor.cpp Source/DrumsProcessor.h
 Source/DrumsEditor.cpp Source/DrumsEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h
 Source/DSP/AbismoCommon.h Source/DSP/AbismoParams.h Source/DSP/AbismoReverb.h Source/DSP/AbismoMutilator.h
 Source/DSP/DrumsParams.h Source/DSP/DrumsEngine.h)
juce_add_plugin(InversoAbismoDrums
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.abismodrums"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvAD
 PRODUCT_NAME "INVERSO ABISMO DRUMS"
 FORMATS VST3
 VST3_CATEGORIES Instrument Drum
 IS_SYNTH TRUE
 NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoAbismoDrums)
target_sources(InversoAbismoDrums PRIVATE ${DRUMS_SOURCES})
target_compile_definitions(InversoAbismoDrums PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoDrums PRIVATE
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 juce::juce_recommended_lto_flags)

juce_add_console_app(InversoAbismoDrumsHostSmoke PRODUCT_NAME "INVERSO Abismo Drums Host Smoke")
juce_generate_juce_header(InversoAbismoDrumsHostSmoke)
target_sources(InversoAbismoDrumsHostSmoke PRIVATE Tests/test_drums_host.cpp ${DRUMS_SOURCES})
target_compile_definitions(InversoAbismoDrumsHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoDrumsHostSmoke PRIVATE
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
add_test(NAME abismo_drums_host_smoke COMMAND InversoAbismoDrumsHostSmoke)
# The drum recordings (CC0, see Library/SOURCES.md) ship inside the plugin bundle,
# in Contents/Resources, so the installer and the portable copy carry them along.
get_target_property(drums_bundle InversoAbismoDrums_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_command(TARGET InversoAbismoDrums_VST3 POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "${drums_bundle}/Contents/Resources"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin" "${drums_bundle}/Contents/Resources/drums.bin"
 VERBATIM)
# Development builds and the host smoke test find the library in the source tree.
target_compile_definitions(InversoAbismoDrums PRIVATE ABISMO_DRUMS_DEV_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin")
target_compile_definitions(InversoAbismoDrumsHostSmoke PRIVATE ABISMO_DRUMS_DEV_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin")
