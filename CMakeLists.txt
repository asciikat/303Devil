cmake_minimum_required(VERSION 3.22)
project(TB303_DevilAcid VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Offline renderer: runs the DSP engine without JUCE and writes WAV files.
add_executable(acid_render tools/render_test.cpp)
target_include_directories(acid_render PRIVATE Source)

option(ACID_BUILD_PLUGIN "Build the VST3/Standalone plugin (downloads JUCE)" ON)

if(ACID_BUILD_PLUGIN)
    include(FetchContent)
    # Release tarball rather than a git clone: smaller download, no history.
    # To use a JUCE copy you already have: -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz)
    FetchContent_MakeAvailable(JUCE)

    # On Windows the VST3 folder needs admin rights, so the build leaves the plugin in
    # build/TB303_DevilAcid_artefacts/Release/VST3 for you to copy instead
    if(WIN32)
        set(ACID_COPY_AFTER_BUILD FALSE)
    else()
        set(ACID_COPY_AFTER_BUILD TRUE)
    endif()

    juce_add_plugin(TB303_DevilAcid
        COMPANY_NAME "DevilAcidAudio"
        BUNDLE_ID com.devilacidaudio.tb303devilacid
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD ${ACID_COPY_AFTER_BUILD}   # Linux: installs the VST3 into ~/.vst3
        PLUGIN_MANUFACTURER_CODE Dvac
        PLUGIN_CODE T303
        VST3_CATEGORIES Instrument Synth
        FORMATS VST3 Standalone
        PRODUCT_NAME "TB-303 Devil Acid Rig")

    target_sources(TB303_DevilAcid PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_include_directories(TB303_DevilAcid PRIVATE Source)

    target_compile_definitions(TB303_DevilAcid PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    # Renders the editor to PNG for checking the GUI without a host
    juce_add_console_app(acid_snapshot PRODUCT_NAME "acid_snapshot")
    target_sources(acid_snapshot PRIVATE
        tools/snapshot.cpp
        tools/follow_tests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(acid_snapshot PRIVATE Source)
    target_compile_definitions(acid_snapshot PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="TB-303 Devil Acid Rig")
    target_link_libraries(acid_snapshot PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)

    # No LTO: link-time optimisation needs more RAM than this machine has spare.
    target_link_libraries(TB303_DevilAcid
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
