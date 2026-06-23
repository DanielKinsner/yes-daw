cmake_minimum_required(VERSION 3.25)
project(YesDaw VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE (fetched + pinned; bump GIT_TAG deliberately to the latest 8.0.x) ---
include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.4
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# --- H0 spike app (throwaway; proves device I/O + the build) ---
juce_add_gui_app(YesDaw
  PRODUCT_NAME "YesDaw"
  COMPANY_NAME "YesDaw")

target_sources(YesDaw PRIVATE src/Main.cpp)

target_compile_definitions(YesDaw PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:YesDaw,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:YesDaw,JUCE_VERSION>")

target_link_libraries(YesDaw
  PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
