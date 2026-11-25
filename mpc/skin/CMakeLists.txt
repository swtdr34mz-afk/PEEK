# The MPC skin's renderer: the VST3's own art (src/ui/Look.cpp) on the MPC's pages, every control cut out at every
# value it can take (film strips, button images, LED rows), with a manifest for make_skin.py.
#   cmake -S mpc/skin -B build-skin -DPEEK_JUCE_DIR=/path/to/JUCE && cmake --build build-skin
#   build-skin/peek_skin_render_artefacts/Release/peek_skin_render mpc/build/skin-render
#   python3 mpc/skin/make_skin.py mpc/build/skin-render mpc/plugin/skins
# (Only when the pages change: the result, mpc/plugin/skins/, is kept in the repository.)
cmake_minimum_required(VERSION 3.22)
project(PeekSkinRender VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(PEEK_JUCE_DIR "" CACHE PATH "a JUCE checkout (empty: from GitHub)")
if(PEEK_JUCE_DIR)
    add_subdirectory(${PEEK_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(R ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_console_app(peek_skin_render PRODUCT_NAME "peek_skin_render")
juce_add_binary_data(PeekSkinData SOURCES
    ${R}/fonts/FiraSans-SemiBold.ttf
    ${R}/fonts/FiraSans-Bold.ttf
    ${R}/fonts/FiraSans-ExtraBold.ttf
    ${R}/fonts/AnonymousPro-Bold.ttf
    ${R}/res/swtdr34mz-tag.png)
target_sources(peek_skin_render PRIVATE render_skin.cpp ${R}/src/PluginProcessor.cpp ${R}/src/PluginEditor.cpp ${R}/src/ui/Look.cpp
    ${R}/dsp/Engine.cpp ${R}/dsp/Params.cpp ${R}/dsp/Presets.cpp ${R}/dsp/Wavetables.cpp ${R}/dsp/Blep.cpp)
target_include_directories(peek_skin_render PRIVATE ${R}/dsp ${R}/src)
target_compile_definitions(peek_skin_render PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XSHM=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0
    JUCE_MODAL_LOOPS_PERMITTED=0 JUCE_STANDALONE_APPLICATION=1 JucePlugin_Build_Standalone=0)
target_link_libraries(peek_skin_render PRIVATE PeekSkinData juce::juce_audio_utils juce::juce_recommended_config_flags)
