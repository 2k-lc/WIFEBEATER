cmake_minimum_required(VERSION 3.22)
project(WIFEBEATER VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(WB_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND WB_FORMATS AU)
endif()

juce_add_plugin(WIFEBEATER
  COMPANY_NAME "WIFEBEATER"
  PRODUCT_NAME "WIFEBEATER"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  PLUGIN_MANUFACTURER_CODE Wfbt
  PLUGIN_CODE Wfb1
  FORMATS ${WB_FORMATS}
  VST3_CATEGORIES Instrument Synth
  COPY_PLUGIN_AFTER_BUILD FALSE)

set(WB_SOURCES
  Source/Params.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_sources(WIFEBEATER PRIVATE ${WB_SOURCES})
target_include_directories(WIFEBEATER PRIVATE Source)
target_compile_definitions(WIFEBEATER PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(WIFEBEATER
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Offline render / sanity test (no audio device needed)
option(WB_BUILD_TESTS "Build offline DSP test" ON)
if(WB_BUILD_TESTS)
  juce_add_console_app(wb_test PRODUCT_NAME "wb_test")
  target_sources(wb_test PRIVATE Tests/RenderTest.cpp ${WB_SOURCES})
  target_include_directories(wb_test PRIVATE Source)
  target_compile_definitions(wb_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 WB_TEST_BUILD=1
    JUCE_STANDALONE_APPLICATION=1)
  target_link_libraries(wb_test PRIVATE juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags)
endif()
