cmake_minimum_required(VERSION 3.22)

project(FrancDelay
    VERSION 1.0.0
    DESCRIPTION "FrancDelay stereo delay audio plugin"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Optional local JUCE checkout. When empty, JUCE 8 is fetched from GitHub.
set(FRANCDELAY_JUCE_PATH "" CACHE PATH
    "Path to a local JUCE checkout. When set, it is added with add_subdirectory instead of FetchContent.")

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_WEB_BROWSER OFF CACHE BOOL "Disable the JUCE web browser (and its Linux webkit dependency)" FORCE)
set(JUCE_USE_CURL OFF CACHE BOOL "Disable JUCE curl support" FORCE)

if(FRANCDELAY_JUCE_PATH)
    message(STATUS "FrancDelay: using local JUCE from ${FRANCDELAY_JUCE_PATH}")
    add_subdirectory("${FRANCDELAY_JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce-local")
else()
    message(STATUS "FrancDelay: fetching JUCE 8.0.15")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(FrancDelay
    COMPANY_NAME "Franc Audio"
    BUNDLE_ID com.francaudio.francdelay
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Frnc
    PLUGIN_CODE FrDl
    FORMATS VST3 Standalone
    PRODUCT_NAME "FrancDelay"
    VST3_CATEGORIES Delay Fx)

target_sources(FrancDelay
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DelayVisualizer.cpp
        Source/DelayVisualizer.h)

target_compile_features(FrancDelay PRIVATE cxx_std_20)
set_target_properties(FrancDelay PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS OFF)

target_compile_definitions(FrancDelay
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

target_link_libraries(FrancDelay
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
