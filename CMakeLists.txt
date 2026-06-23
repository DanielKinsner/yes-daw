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

# --- Warnings-as-errors, scoped to OUR targets only -------------------------------
# CLAUDE.md makes warnings-as-errors a mechanical gate. We apply JUCE's own recommended
# warning set (the set JUCE keeps its headers clean against) and turn it fatal for our
# targets only — never for JUCE's own targets, which we leave untouched.
function(yesdaw_strict_juce target)
  target_link_libraries(${target} PRIVATE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
  set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endfunction()

# Project headers under src/ are includable as "dsp/SineSource.h".
set(YESDAW_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- H0 spike app (throwaway; proves device I/O + the build) ---
juce_add_gui_app(YesDaw
  PRODUCT_NAME "YesDaw"
  COMPANY_NAME "YesDaw")

target_sources(YesDaw PRIVATE src/Main.cpp)
target_include_directories(YesDaw PRIVATE ${YESDAW_SRC_DIR})

target_compile_definitions(YesDaw PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:YesDaw,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:YesDaw,JUCE_VERSION>")

target_link_libraries(YesDaw
  PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra)

yesdaw_strict_juce(YesDaw)

# --- Headless DSP self-check (the mechanical gate; runs in CI, no device/display) ---
# Pure C++ — depends ONLY on src/dsp, never on JUCE — so it builds and runs anywhere fast.
enable_testing()

add_executable(YesDawTests tests/dsp_tests.cpp)
target_include_directories(YesDawTests PRIVATE ${YESDAW_SRC_DIR})

# Absolute path to the golden so the test doesn't depend on the working directory ctest runs in.
target_compile_definitions(YesDawTests PRIVATE
  YESDAW_GOLDEN_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/sine_440_48k.csv")

# Warnings-as-errors with a portable, locally-verifiable warning set (MSVC /W4 is the strictest
# we can check on the Windows dev box; macOS CI uses the weaker -Wall -Wextra, so a local-clean
# build de-risks the runner we can't see).
set_target_properties(YesDawTests PROPERTIES COMPILE_WARNING_AS_ERROR ON)
target_compile_options(YesDawTests PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>)

add_test(NAME dsp_tests COMMAND YesDawTests)
