cmake_minimum_required(VERSION 3.22)
project(MIX_PRO VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MIXPRO_BUILD_PLUGIN "Build the JUCE VST3 + Standalone (downloads JUCE)" ON)
option(MIXPRO_BUILD_TESTS  "Build the JUCE-free DSP/analysis/preset test-suite" ON)

# JUCE-free core: DSP, analysis, engines, parameter table, presets
add_library(mixpro_core INTERFACE)
target_include_directories(mixpro_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if(MIXPRO_BUILD_TESTS)
  enable_testing()
  add_executable(MixProCoreTests Tests/test_core.cpp)
  target_link_libraries(MixProCoreTests PRIVATE mixpro_core)
  if(MSVC)
    target_compile_options(MixProCoreTests PRIVATE /W3 /utf-8 /bigobj)
  else()
    target_compile_options(MixProCoreTests PRIVATE -Wall -Wextra -O2)
  endif()
  add_test(NAME MixProCoreTests COMMAND MixProCoreTests)
endif()

if(MIXPRO_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4            # pinned
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(MixPro
    COMPANY_NAME "ORYMY"
    PRODUCT_NAME "MIX PRO"
    BUNDLE_ID com.orymy.mixpro
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Orym
    PLUGIN_CODE MxPr
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Dynamics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(MixPro PRIVATE Source/Plugin/PluginProcessor.cpp Source/Plugin/PluginEditor.cpp)
  target_compile_definitions(MixPro PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_MP3AUDIOFORMAT=1)
  target_link_libraries(MixPro
    PRIVATE mixpro_core juce::juce_audio_utils juce::juce_audio_formats juce::juce_audio_processors
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
