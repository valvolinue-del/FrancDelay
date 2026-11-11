cmake_minimum_required(VERSION 3.22)

# CMake 3.28+ scans C++20 sources for modules. JUCE is not a module project, and
# that scan fails the MSVC build. Set this before project() so the compiler
# check and every target, including the fetched JUCE build, inherit it.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "" FORCE)

# arm64 cannot deploy earlier than macOS 11. The CI universal build
# (arm64 + x86_64) therefore uses 11.0. This must be set before project() so
# the compiler test and the fetched JUCE build inherit it. Ignored on
# non-Apple hosts. Override with -DCMAKE_OSX_DEPLOYMENT_TARGET=...
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS version. Universal arm64 builds cannot go below 11.0.")
endif()

project(FrancDelay
    VERSION 1.0.0
    DESCRIPTION "FrancDelay stereo delay audio plugin"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # __cplusplus stays at 199711L unless /Zc:__cplusplus is set. /permissive-
    # is the conforming mode the designated initializers in this project need.
    # /utf-8 matches the source files on disk. /bigobj avoids C1128 in unity
    # builds that overflow the COFF section limit (JUCE also requests it for
    # its larger modules). Nothing here is Linux-specific.
    add_compile_options(
        /utf-8
        /bigobj
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
endif()

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

if(MSVC)
    # juce_recommended_lto_flags compiles Release with /GL and forwards -LTCG
    # through target_link_libraries. Also pass the linker's own /LTCG spelling
    # so the flag survives the Visual Studio generator, and propagate it to the
    # VST3 and Standalone wrappers. Debug builds stay incremental.
    target_compile_options(FrancDelay PRIVATE $<$<CONFIG:Release>:/GL>)
    target_link_options(FrancDelay PUBLIC $<$<CONFIG:Release>:/LTCG>)
endif()
