cmake_minimum_required(VERSION 3.22)
project(InversoDrum VERSION 0.5.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(INVERSO_DSP_TESTS_ONLY "Only build portable sequencer tests" OFF)
enable_testing()
add_executable(inverso_drum_engine Tests/test_engine.cpp Source/DrumEngine.h Source/DrumFX.h)
add_executable(inverso_drum_extended Tests/test_extended.cpp Source/DrumEngine.h Source/DrumFX.h)
add_executable(inverso_drum_fx_audible Tests/test_fx_audible.cpp Source/DrumEngine.h Source/DrumFX.h)
add_executable(inverso_drum_velocity Tests/test_velocity.cpp Source/DrumEngine.h)
add_executable(inverso_drum_event_fx Tests/test_event_fx.cpp Source/DrumEngine.h)
if(MSVC)
 target_compile_options(inverso_drum_engine PRIVATE /W4)
 target_compile_options(inverso_drum_extended PRIVATE /W4)
 target_compile_options(inverso_drum_fx_audible PRIVATE /W4)
 target_compile_options(inverso_drum_velocity PRIVATE /W4)
 target_compile_options(inverso_drum_event_fx PRIVATE /W4)
else()
 target_compile_options(inverso_drum_engine PRIVATE -Wall -Wextra -Wpedantic)
 target_compile_options(inverso_drum_extended PRIVATE -Wall -Wextra -Wpedantic)
 target_compile_options(inverso_drum_fx_audible PRIVATE -Wall -Wextra -Wpedantic)
 target_compile_options(inverso_drum_velocity PRIVATE -Wall -Wextra -Wpedantic)
 target_compile_options(inverso_drum_event_fx PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME inverso_drum_engine COMMAND inverso_drum_engine)
add_test(NAME inverso_drum_extended COMMAND inverso_drum_extended)
add_test(NAME inverso_drum_fx_audible COMMAND inverso_drum_fx_audible)
add_test(NAME inverso_drum_velocity COMMAND inverso_drum_velocity)
add_test(NAME inverso_drum_event_fx COMMAND inverso_drum_event_fx)
if(INVERSO_DSP_TESTS_ONLY)
 return()
endif()
if(DEFINED JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
 add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
 find_package(JUCE CONFIG REQUIRED)
endif()
juce_add_plugin(InversoDrum
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.drum"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvDm
 PRODUCT_NAME "INVERSO DRUM"
 FORMATS VST3
 VST3_CATEGORIES Instrument
 IS_SYNTH TRUE
 NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoDrum)
target_sources(InversoDrum PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h Source/SampleLibraries.h Source/DrumEngine.h Source/DrumFX.h)
target_compile_definitions(InversoDrum PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoDrum PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
juce_add_console_app(InversoDrumHostSmoke PRODUCT_NAME "Inverso Drum Host Smoke")
juce_generate_juce_header(InversoDrumHostSmoke)
target_sources(InversoDrumHostSmoke PRIVATE Tests/test_host.cpp Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h Source/SampleLibraries.h Source/DrumEngine.h Source/DrumFX.h)
target_compile_definitions(InversoDrumHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoDrumHostSmoke PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
add_test(NAME inverso_drum_host_smoke COMMAND InversoDrumHostSmoke)
