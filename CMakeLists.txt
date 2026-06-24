cmake_minimum_required(VERSION 3.25)
project(YesDaw VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clang-format / clang-tidy / editors consume this

# YESDAW_BUILD_APPS=OFF configures ONLY the pure Catch2 test (no JUCE) — used by the RTSan CI leg so
# it needs neither the JUCE clone nor the X11/ALSA dev libs that JUCE's GUI/audio modules require.
option(YESDAW_BUILD_APPS "Build the JUCE GUI app + soak tool (off => pure-test-only)" ON)

# YESDAW_SANITIZER plumbs in the RTSan CI leg (-fsanitize=realtime) that enforces
# "the audio thread never allocates/locks".
option(YESDAW_SANITIZER "asan | undefined | thread | realtime  (empty = off)" "")

# --- Dependencies (fetched + pinned to a tag; bump deliberately) ------------------
# SYSTEM demotes their headers to -isystem / /external:I, so our warnings-as-errors
# (yesdaw_harden below) applies to OUR code only and never trips on a JUCE/Catch2 header.
include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
  GIT_SHALLOW    TRUE
  SYSTEM)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.4
  GIT_SHALLOW    TRUE
  SYSTEM)

if (YESDAW_BUILD_APPS)
  FetchContent_MakeAvailable(JUCE Catch2)
else()
  FetchContent_MakeAvailable(Catch2)   # RTSan leg: skip the JUCE clone entirely
endif()

# --- Hardening: warnings-as-errors on OUR code only + an optional sanitizer --------
# CLAUDE.md makes warnings-as-errors a mechanical gate. JUCE/Catch2 are SYSTEM (above),
# so only our own code is held to the bar.

function(yesdaw_harden target)
  if (MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()

  if (YESDAW_SANITIZER)
    # The sanitizer leg's job is RUNTIME real-time safety, not warning-cleanliness (the normal matrix
    # already enforces warnings-as-errors). We deliberately do NOT add -Werror here: the
    # [[clang::nonblocking]] attribute turns on a *static* -Wfunction-effects check that flags any call
    # to an unannotated function (std::sin, std::min, ...), which is too strict for RT-safe stdlib math.
    # RTSan's runtime detection — which actually intercepts allocations/locks/syscalls — is the gate.
    target_compile_options(${target} PRIVATE -fsanitize=${YESDAW_SANITIZER} -fno-omit-frame-pointer)
    target_link_options   (${target} PRIVATE -fsanitize=${YESDAW_SANITIZER})
  elseif (MSVC)
    target_compile_options(${target} PRIVATE /WX)
  else()
    target_compile_options(${target} PRIVATE -Werror)
  endif()
endfunction()

# Project headers under src/ are includable as "dsp/SineSource.h".
set(YESDAW_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (YESDAW_BUILD_APPS)

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

# --- Real-machine soak (H0 exit gate; built everywhere, RUN by the owner on real hardware) ---
# Console app, links only juce_audio_devices (no GUI -> ALSA-only on Linux). CI compile-checks it on
# all three OSes; it is never run in CI (no audio device). See tools/soak.sh.
juce_add_console_app(YesDawSoak PRODUCT_NAME "YesDawSoak")
target_sources(YesDawSoak PRIVATE tools/soak/SoakMain.cpp)
target_include_directories(YesDawSoak PRIVATE ${YESDAW_SRC_DIR})
target_compile_definitions(YesDawSoak PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(YesDawSoak
  PRIVATE
    juce::juce_audio_devices
    juce::juce_recommended_config_flags)
yesdaw_harden(YesDawSoak)

endif() # YESDAW_BUILD_APPS

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
