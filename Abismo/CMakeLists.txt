cmake_minimum_required(VERSION 3.22)
project(InversoAbismo VERSION 0.17.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(INVERSO_DSP_TESTS_ONLY "Build portable DSP tests without JUCE" OFF)
enable_testing()
add_executable(abismo_engine Tests/test_engine.cpp)
add_executable(abismo_presets Tests/test_presets.cpp)
add_executable(abismo_stress Tests/test_stress.cpp)
add_executable(abismo_sampler Tests/test_sampler.cpp)
add_executable(abismo_drums Tests/test_drums.cpp)
add_executable(relicario_engine Tests/test_relicario.cpp)
add_executable(enki_engine Tests/test_enki.cpp)
add_executable(furnace_engine Tests/test_furnace.cpp)
add_executable(clear_engine Tests/test_clear.cpp)
add_executable(tide_engine Tests/test_tide.cpp)
foreach(t IN ITEMS abismo_engine abismo_presets abismo_stress abismo_sampler abismo_drums relicario_engine enki_engine furnace_engine clear_engine tide_engine)
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
add_test(NAME relicario_engine COMMAND relicario_engine)
add_test(NAME enki_engine COMMAND enki_engine)
add_test(NAME furnace_engine COMMAND furnace_engine)
add_test(NAME clear_engine COMMAND clear_engine)
add_test(NAME tide_engine COMMAND tide_engine)
target_compile_definitions(abismo_drums PRIVATE ABISMO_DRUMS_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin")
target_compile_definitions(enki_engine PRIVATE ABISMO_DRUMS_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin")
if(INVERSO_DSP_TESTS_ONLY)
 return()
endif()
# Link-time optimisation for the plugins. The DSP of each plugin sits in one
# translation unit, so turning it off (as CI does to save build minutes) costs
# almost nothing at run time.
option(INVERSO_LTO "Link-time optimisation for the plugins" ON)
if(DEFINED JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
 add_subdirectory("${JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
 find_package(JUCE CONFIG REQUIRED)
endif()
set(ABISMO_SOURCES
 Source/PluginProcessor.cpp Source/PluginProcessor.h
 Source/PluginEditor.cpp Source/PluginEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
 Source/DSP/AbismoCommon.h Source/DSP/AbismoParams.h Source/DSP/AbismoEngine.h Source/DSP/AbismoReverb.h Source/DSP/AbismoMutilator.h)
# Embedded typefaces (SIL OFL, see Fonts/): Poppins and IBM Plex Mono for the soft UI; Oswald,
# Bebas Neue and Courier Prime for the vintage UI.
juce_add_binary_data(AbismoFonts HEADER_NAME AbismoFonts.h NAMESPACE AbismoFonts
 SOURCES Fonts/Poppins-Medium.ttf Fonts/Poppins-SemiBold.ttf Fonts/IBMPlexMono-Medium.ttf
 Fonts/Oswald-Medium.ttf Fonts/Oswald-SemiBold.ttf Fonts/BebasNeue-Regular.ttf Fonts/CourierPrime-Bold.ttf)
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
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)

# Drives the JUCE processor like a DAW: MIDI at varying block sizes, programs,
# automation and state save/restore. Builds on the Windows runner.
juce_add_console_app(InversoAbismoHostSmoke PRODUCT_NAME "INVERSO Abismo Host Smoke")
juce_generate_juce_header(InversoAbismoHostSmoke)
target_sources(InversoAbismoHostSmoke PRIVATE Tests/test_host.cpp ${ABISMO_SOURCES})
target_compile_definitions(InversoAbismoHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
add_test(NAME abismo_host_smoke COMMAND InversoAbismoHostSmoke)

# ABISMO SAMPLER: second instrument built from the same DSP and UI blocks.
set(SAMPLER_SOURCES
 Source/SamplerProcessor.cpp Source/SamplerProcessor.h
 Source/SamplerEditor.cpp Source/SamplerEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
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
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)

juce_add_console_app(InversoAbismoSamplerHostSmoke PRODUCT_NAME "INVERSO Abismo Sampler Host Smoke")
juce_generate_juce_header(InversoAbismoSamplerHostSmoke)
target_sources(InversoAbismoSamplerHostSmoke PRIVATE Tests/test_sampler_host.cpp ${SAMPLER_SOURCES})
target_compile_definitions(InversoAbismoSamplerHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoSamplerHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
add_test(NAME abismo_sampler_host_smoke COMMAND InversoAbismoSamplerHostSmoke)

# ABISMO DRUMS: synthesised junkyard percussion and musical saw.
set(DRUMS_SOURCES
 Source/DrumsProcessor.cpp Source/DrumsProcessor.h
 Source/DrumsEditor.cpp Source/DrumsEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
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
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)

juce_add_console_app(InversoAbismoDrumsHostSmoke PRODUCT_NAME "INVERSO Abismo Drums Host Smoke")
juce_generate_juce_header(InversoAbismoDrumsHostSmoke)
target_sources(InversoAbismoDrumsHostSmoke PRIVATE Tests/test_drums_host.cpp ${DRUMS_SOURCES})
target_compile_definitions(InversoAbismoDrumsHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoAbismoDrumsHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
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

# RELICARIO: four forgotten machines (tape loops, reed organ, tuning fork, Soviet organ).
set(RELICARIO_SOURCES
 Source/RelicarioProcessor.cpp Source/RelicarioProcessor.h
 Source/RelicarioEditor.cpp Source/RelicarioEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
 Source/DSP/AbismoCommon.h Source/DSP/AbismoParams.h Source/DSP/AbismoReverb.h
 Source/DSP/RelicarioParams.h Source/DSP/RelicarioEngine.h)
juce_add_plugin(InversoRelicario
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.relicario"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvRl
 PRODUCT_NAME "INVERSO RELICARIO"
 FORMATS VST3
 VST3_CATEGORIES Instrument Synth
 IS_SYNTH TRUE
 NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoRelicario)
target_sources(InversoRelicario PRIVATE ${RELICARIO_SOURCES})
target_compile_definitions(InversoRelicario PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoRelicario PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)
juce_add_console_app(InversoRelicarioHostSmoke PRODUCT_NAME "INVERSO Relicario Host Smoke")
juce_generate_juce_header(InversoRelicarioHostSmoke)
target_sources(InversoRelicarioHostSmoke PRIVATE Tests/test_relicario_host.cpp ${RELICARIO_SOURCES})
target_compile_definitions(InversoRelicarioHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoRelicarioHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
add_test(NAME relicario_host_smoke COMMAND InversoRelicarioHostSmoke)

# ENKI: a sound idea generator (two sources, shaping, motion, space; GENERATE and MUTATE).
set(ENKI_SOURCES
 Source/EnkiProcessor.cpp Source/EnkiProcessor.h
 Source/EnkiEditor.cpp Source/EnkiEditor.h
 Source/EnkiLibrary.cpp Source/EnkiLibrary.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
 Source/DSP/AbismoCommon.h Source/DSP/AbismoReverb.h Source/DSP/DrumsEngine.h
 Source/DSP/EnkiParams.h Source/DSP/EnkiEngine.h Source/DSP/EnkiGenerator.h Source/DSP/RelicarioFx.h)
set(ENKI_DEFINITIONS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_MP3AUDIOFORMAT=1)
juce_add_plugin(InversoEnki
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.enki"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvEn
 PRODUCT_NAME "INVERSO ENKI"
 FORMATS VST3
 VST3_CATEGORIES Instrument Synth
 IS_SYNTH TRUE
 NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS TRUE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoEnki)
target_sources(InversoEnki PRIVATE ${ENKI_SOURCES})
target_compile_definitions(InversoEnki PUBLIC ${ENKI_DEFINITIONS})
target_link_libraries(InversoEnki PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)
juce_add_console_app(InversoEnkiHostSmoke PRODUCT_NAME "INVERSO Enki Host Smoke")
juce_generate_juce_header(InversoEnkiHostSmoke)
target_sources(InversoEnkiHostSmoke PRIVATE Tests/test_enki_host.cpp ${ENKI_SOURCES})
target_compile_definitions(InversoEnkiHostSmoke PRIVATE ${ENKI_DEFINITIONS})
target_link_libraries(InversoEnkiHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
add_test(NAME enki_host_smoke COMMAND InversoEnkiHostSmoke)
# ENKI plays the drum recordings too: they ship in its bundle as well.
get_target_property(enki_bundle InversoEnki_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_command(TARGET InversoEnki_VST3 POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "${enki_bundle}/Contents/Resources"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin" "${enki_bundle}/Contents/Resources/drums.bin"
 VERBATIM)
target_compile_definitions(InversoEnki PRIVATE ABISMO_DRUMS_DEV_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin")
target_compile_definitions(InversoEnkiHostSmoke PRIVATE ABISMO_DRUMS_DEV_LIBRARY="${CMAKE_CURRENT_SOURCE_DIR}/Library/drums.bin")

# FURNACE: guitar and bass amplifiers with the INVERSO CABS inside (an effect).
set(FURNACE_SOURCES
 Source/FurnaceProcessor.cpp Source/FurnaceProcessor.h
 Source/FurnaceEditor.cpp Source/FurnaceEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
 Source/DSP/AbismoCommon.h Source/DSP/FurnaceParams.h Source/DSP/FurnaceAmp.h Source/DSP/FurnaceCab.h)
juce_add_plugin(InversoFurnace
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.furnace"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvFn
 PRODUCT_NAME "INVERSO FURNACE"
 FORMATS VST3
 VST3_CATEGORIES Fx Distortion
 IS_SYNTH FALSE
 NEEDS_MIDI_INPUT FALSE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoFurnace)
target_sources(InversoFurnace PRIVATE ${FURNACE_SOURCES})
target_compile_definitions(InversoFurnace PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoFurnace PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)
juce_add_console_app(InversoFurnaceHostSmoke PRODUCT_NAME "INVERSO Furnace Host Smoke")
juce_generate_juce_header(InversoFurnaceHostSmoke)
target_sources(InversoFurnaceHostSmoke PRIVATE Tests/test_furnace_host.cpp ${FURNACE_SOURCES})
target_compile_definitions(InversoFurnaceHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoFurnaceHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
add_test(NAME furnace_host_smoke COMMAND InversoFurnaceHostSmoke)

# CLEAR: takes the mud and the harshness out of a sound only when they are there (an effect).
set(CLEAR_SOURCES
 Source/ClearProcessor.cpp Source/ClearProcessor.h
 Source/ClearEditor.cpp Source/ClearEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
 Source/DSP/ClearParams.h Source/DSP/ClearEngine.h)
juce_add_plugin(InversoClear
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.clear"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvCl
 PRODUCT_NAME "INVERSO CLEAR"
 FORMATS VST3
 VST3_CATEGORIES Fx EQ
 IS_SYNTH FALSE
 NEEDS_MIDI_INPUT FALSE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoClear)
target_sources(InversoClear PRIVATE ${CLEAR_SOURCES})
target_compile_definitions(InversoClear PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoClear PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)
juce_add_console_app(InversoClearHostSmoke PRODUCT_NAME "INVERSO Clear Host Smoke")
juce_generate_juce_header(InversoClearHostSmoke)
target_sources(InversoClearHostSmoke PRIVATE Tests/test_clear_host.cpp ${CLEAR_SOURCES})
target_compile_definitions(InversoClearHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoClearHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
add_test(NAME clear_host_smoke COMMAND InversoClearHostSmoke)

# TIDE: a dynamic EQ for instruments (an effect).
set(TIDE_SOURCES
 Source/TideProcessor.cpp Source/TideProcessor.h
 Source/TideEditor.cpp Source/TideEditor.h
 Source/AbismoUI.cpp Source/AbismoUI.h Source/SoftUI.cpp Source/SoftUI.h
 Source/DSP/TideParams.h Source/DSP/TideEngine.h)
juce_add_plugin(InversoTide
 COMPANY_NAME "Inverso Studios"
 BUNDLE_ID "net.inversostudios.tide"
 PLUGIN_MANUFACTURER_CODE InvS
 PLUGIN_CODE IvTd
 PRODUCT_NAME "INVERSO TIDE"
 FORMATS VST3
 VST3_CATEGORIES Fx EQ
 IS_SYNTH FALSE
 NEEDS_MIDI_INPUT FALSE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(InversoTide)
target_sources(InversoTide PRIVATE ${TIDE_SOURCES})
target_compile_definitions(InversoTide PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoTide PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp
 juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags
 $<$<BOOL:${INVERSO_LTO}>:juce::juce_recommended_lto_flags>)
juce_add_console_app(InversoTideHostSmoke PRODUCT_NAME "INVERSO Tide Host Smoke")
juce_generate_juce_header(InversoTideHostSmoke)
target_sources(InversoTideHostSmoke PRIVATE Tests/test_tide_host.cpp ${TIDE_SOURCES})
target_compile_definitions(InversoTideHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(InversoTideHostSmoke PRIVATE
 AbismoFonts
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)
add_test(NAME tide_host_smoke COMMAND InversoTideHostSmoke)
