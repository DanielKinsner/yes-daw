cmake_minimum_required(VERSION 3.25)
project(YesDaw VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clang-format / clang-tidy / editors consume this

# --- Dependencies (fetched + pinned to a tag; bump deliberately) ------------------
# SYSTEM demotes their headers to -isystem / /external:I, so our warnings-as-errors
# (yesdaw_harden below) applies to OUR code only and never trips on a JUCE/Catch2 header.
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.4
  GIT_SHALLOW    TRUE
  SYSTEM)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
  GIT_SHALLOW    TRUE
  SYSTEM)
FetchContent_MakeAvailable(JUCE Catch2)

# --- Hardening: warnings-as-errors on OUR code only + an optional sanitizer --------
# CLAUDE.md makes warnings-as-errors a mechanical gate. JUCE/Catch2 are SYSTEM (above),
# so only our own code is held to the bar. YESDAW_SANITIZER plumbs in the RTSan CI leg
# (-fsanitize=realtime) that enforces "the audio thread never allocates/locks".
option(YESDAW_SANITIZER "asan | undefined | thread | realtime  (empty = off)" "")

function(yesdaw_harden target)
  if (MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
  if (YESDAW_SANITIZER)
    target_compile_options(${target} PRIVATE -fsanitize=${YESDAW_SANITIZER} -fno-omit-frame-pointer)
    target_link_options   (${target} PRIVATE -fsanitize=${YESDAW_SANITIZER})
  endif()
endfunction()

# Project headers under src/ are includable as "dsp/SineSource.h".
set(YESDAW_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- H0 spike app (throwaway GUI; proves device I/O + the build) ------------------
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
    juce::juce_gui_extra
    juce::juce_recommended_config_flags)

yesdaw_harden(YesDaw)

# --- Headless DSP self-check (the mechanical gate; runs in CI, no device/display) ---
# Pure C++ + Catch2 — depends ONLY on src/dsp, never on JUCE — so it builds/runs fast anywhere.
enable_testing()

add_executable(YesDawCheck tests/dsp_tests.cpp)
target_include_directories(YesDawCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawCheck PRIVATE Catch2::Catch2)

# Absolute path to the golden so the test doesn't depend on ctest's working directory.
target_compile_definitions(YesDawCheck PRIVATE
  YESDAW_GOLDEN_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/sine_440_48k.csv")

yesdaw_harden(YesDawCheck)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(YesDawCheck)

# Bless the golden after an INTENTIONAL DSP change (review the diff before committing):
#   cmake --build build --target bless-goldens
add_custom_target(bless-goldens
  COMMAND $<TARGET_FILE:YesDawCheck> --emit-golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/sine_440_48k.csv"
  DEPENDS YesDawCheck
  VERBATIM
  COMMENT "Regenerating goldens from the current DSP")
