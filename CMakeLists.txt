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
# choc: header-only, ISC. We use choc::fifo::SingleReaderSingleWriterFIFO as the ONE sanctioned
# lock-free SPSC primitive (ADR-0006 / deepening-notes ADOPT #1 — "do not hand-roll three"). Pinned by
# commit (no semver tags upstream); GIT_SHALLOW FALSE so the exact SHA stays fetchable as HEAD moves.
# Pure C++, so it is fetched on the RTSan/TSan legs too (it carries no JUCE/X11/ALSA dependency).
FetchContent_Declare(choc
  GIT_REPOSITORY https://github.com/Tracktion/choc.git
  GIT_TAG        5685fb59db9e60d31a35df7bff0ce7967beaaf25
  GIT_SHALLOW    FALSE
  SYSTEM)
FetchContent_Declare(sqlite3
  URL       https://www.sqlite.org/2026/sqlite-amalgamation-3530200.zip
  URL_HASH SHA3_256=81142986038e18f96c4a54e1a72562ae17e502a916f2a7701eff43388cbf1a40
  SYSTEM)

if (YESDAW_BUILD_APPS)
  FetchContent_MakeAvailable(JUCE Catch2 choc sqlite3)
else()
  FetchContent_MakeAvailable(Catch2 choc sqlite3)   # RTSan/TSan leg: skip the JUCE clone, still get choc
endif()

add_library(sqlite3_amalgamation STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_amalgamation SYSTEM PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(sqlite3_amalgamation PRIVATE
  SQLITE_DQS=0
  SQLITE_OMIT_LOAD_EXTENSION
  SQLITE_THREADSAFE=1)

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
    if (YESDAW_SANITIZER STREQUAL "realtime")
      # Bind all symbols at load time. Otherwise the FIRST call to a lazily-resolved libm symbol
      # (std::sin) runs the dynamic linker's PLT resolver INSIDE the [[clang::nonblocking]] hot path,
      # and RTSan flags that resolver's internal lock — an intermittent false failure that surfaced
      # only depending on Catch2's randomized test order.
      target_link_options(${target} PRIVATE -Wl,-z,now)
    endif()
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

# std::thread in the Runtime stress test needs the platform threads library (pthread on Linux/macOS).
find_package(Threads REQUIRED)

if (YESDAW_BUILD_APPS)
  # --- Plugin host worker (H3; ADR-0015 process-model shell) ---
  # This is only the child executable boundary. It owns JUCE plugin-hosting modules; the engine/pure
  # targets stay hosting-free and the layering assertion near the end of this file enforces that.
  juce_add_console_app(YesDawPluginHost PRODUCT_NAME "YesDawPluginHost")
  target_sources(YesDawPluginHost PRIVATE src/plugin_host/PluginHostMain.cpp)
  target_include_directories(YesDawPluginHost PRIVATE ${YESDAW_SRC_DIR})
  target_compile_definitions(YesDawPluginHost PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0)
  target_link_libraries(YesDawPluginHost
    PRIVATE
      juce::juce_audio_processors
      juce::juce_recommended_config_flags)
  if (APPLE)
    target_link_libraries(YesDawPluginHost PRIVATE "-framework AudioUnit" "-framework CoreAudioKit")
  endif()
  yesdaw_harden(YesDawPluginHost)
  add_test(NAME YesDawPluginHostSelfCheck COMMAND $<TARGET_FILE:YesDawPluginHost> --self-check)

  # --- Plugin host coordinator launch/handshake shell (H3; ADR-0015 control lane) ---
  # The coordinator owns process launch + JUCE's control message channel, but not plugin-hosting modules.
  # This self-check proves the worker boundary with a ready/echo handshake; no plugin load, scanner,
  # blacklist/cache, real shared memory, plugin watchdog policy, or UI embedding is introduced here.
  juce_add_console_app(YesDawPluginHostCoordinatorCheck PRODUCT_NAME "YesDawPluginHostCoordinatorCheck")
  target_sources(YesDawPluginHostCoordinatorCheck PRIVATE src/plugin_host/PluginHostCoordinatorCheck.cpp)
  target_include_directories(YesDawPluginHostCoordinatorCheck PRIVATE ${YESDAW_SRC_DIR})
  target_compile_definitions(YesDawPluginHostCoordinatorCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(YesDawPluginHostCoordinatorCheck
    PRIVATE
      juce::juce_events
      juce::juce_recommended_config_flags)
  add_dependencies(YesDawPluginHostCoordinatorCheck YesDawPluginHost)
  yesdaw_harden(YesDawPluginHostCoordinatorCheck)
  add_test(NAME YesDawPluginHostCoordinatorCheck
           COMMAND $<TARGET_FILE:YesDawPluginHostCoordinatorCheck> $<TARGET_FILE:YesDawPluginHost>)

  # --- H3 close-out exit gate ----------------------------------------------------
  # Starts as a Catch2 [!shouldfail] target so main remains green while the host-isolation
  # finish line is honestly red. Flip the test to blocking when the hosted-process path exists.
  add_executable(YesDawHostIsolationCheck tests/host_isolation_tests.cpp)
  target_include_directories(YesDawHostIsolationCheck PRIVATE ${YESDAW_SRC_DIR})
  target_link_libraries(YesDawHostIsolationCheck PRIVATE Catch2::Catch2WithMain)
  yesdaw_harden(YesDawHostIsolationCheck)
  add_test(NAME YesDawHostIsolationCheck COMMAND $<TARGET_FILE:YesDawHostIsolationCheck>)
endif()

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

# --- Timeline layout check (spike #2; pure viewport virtualization, no JUCE/GPU) ---
add_executable(YesDawUiCheck tests/timeline_tests.cpp)
target_include_directories(YesDawUiCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawUiCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawUiCheck)
catch_discover_tests(YesDawUiCheck)

# --- Node-trait check (spike #3; pure, proves block-size-independent processing) ---
add_executable(YesDawEngineCheck tests/node_tests.cpp)
target_include_directories(YesDawEngineCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawEngineCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawEngineCheck)
catch_discover_tests(YesDawEngineCheck)

# --- Time-model check (H1; pure, ADR-0010 storage-facing value types) ---
add_executable(YesDawTimeCheck tests/time_tests.cpp)
target_include_directories(YesDawTimeCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawTimeCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawTimeCheck)
catch_discover_tests(YesDawTimeCheck)

# --- Event-stream check (H1; pure, ADR-0009 generic block-sliced events + param changes) ---
add_executable(YesDawEventCheck tests/event_tests.cpp)
target_include_directories(YesDawEventCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawEventCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawEventCheck)
catch_discover_tests(YesDawEventCheck)

# --- Project value-surface check (H1; pure, ADR-0011 EntityId + Asset/Clip/Project values) ---
add_executable(YesDawProjectCheck tests/project_tests.cpp)
target_include_directories(YesDawProjectCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawProjectCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawProjectCheck)
catch_discover_tests(YesDawProjectCheck)

# --- Persistence check (H1; SQLite .yesdaw bundle schema/migrations/intent log — ADR-0012) ---
add_executable(YesDawPersistenceCheck tests/persistence_tests.cpp)
target_include_directories(YesDawPersistenceCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawPersistenceCheck PRIVATE Catch2::Catch2WithMain sqlite3_amalgamation)
yesdaw_harden(YesDawPersistenceCheck)
catch_discover_tests(YesDawPersistenceCheck)

# --- CompiledGraph check (H1; pure, the immutable graph the audio thread runs — ADR-0006/0007) ---
add_executable(YesDawGraphCheck tests/compiledgraph_tests.cpp)
target_include_directories(YesDawGraphCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawGraphCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawGraphCheck)
catch_discover_tests(YesDawGraphCheck)

# --- GraphBuilder check (H1; pure, compiler Pass 1+2 + first real render — ADR-0007) ---
add_executable(YesDawBuilderCheck tests/builder_tests.cpp)
target_include_directories(YesDawBuilderCheck PRIVATE ${YESDAW_SRC_DIR})
target_compile_definitions(YesDawBuilderCheck PRIVATE YESDAW_TEST_DEBUG_POOL_PAINT=1)
target_link_libraries(YesDawBuilderCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawBuilderCheck)
catch_discover_tests(YesDawBuilderCheck)

# --- Runtime check (H1; pure, RT-safe graph publish/swap + generation-counter reclamation — ADR-0006) ---
# Built UNCONDITIONALLY so the RTSan leg (-fsanitize=realtime) covers processBlock and the TSan leg
# (-fsanitize=thread) covers the publish/process/reclaim data race. Uses choc's SPSC FIFO + std::thread.
add_executable(YesDawRuntimeCheck tests/runtime_tests.cpp)
target_include_directories(YesDawRuntimeCheck PRIVATE ${YESDAW_SRC_DIR})
target_include_directories(YesDawRuntimeCheck SYSTEM PRIVATE ${choc_SOURCE_DIR})
target_link_libraries(YesDawRuntimeCheck PRIVATE Catch2::Catch2WithMain Threads::Threads)
yesdaw_harden(YesDawRuntimeCheck)
catch_discover_tests(YesDawRuntimeCheck)

# --- Render equivalence check (H1; RT path vs same-Project offline Render path) ---
add_executable(YesDawRenderCheck tests/render_tests.cpp)
target_include_directories(YesDawRenderCheck PRIVATE ${YESDAW_SRC_DIR})
target_include_directories(YesDawRenderCheck SYSTEM PRIVATE ${choc_SOURCE_DIR})
target_link_libraries(YesDawRenderCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawRenderCheck)
catch_discover_tests(YesDawRenderCheck)

# --- DelayNode check (H1; pure, the ONE delay primitive for PDC + feedback — ADR-0007 #5) ---
add_executable(YesDawDelayCheck tests/delaynode_tests.cpp)
target_include_directories(YesDawDelayCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawDelayCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawDelayCheck)
catch_discover_tests(YesDawDelayCheck)

# --- FaderNode check (H1; pure, ramped linear gain behind the Node contract — ADR-0008) ---
add_executable(YesDawFaderCheck tests/fader_tests.cpp)
target_include_directories(YesDawFaderCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawFaderCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawFaderCheck)
catch_discover_tests(YesDawFaderCheck)

# --- PanNode check (H1; pure, equal-power mono->stereo placement — ADR-0008) ---
add_executable(YesDawPanCheck tests/pan_tests.cpp)
target_include_directories(YesDawPanCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawPanCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawPanCheck)
catch_discover_tests(YesDawPanCheck)

# --- SumNode check (H1; pure, f64 Bus summing + canonical order — ADR-0007 #6) ---
add_executable(YesDawSumCheck tests/sum_tests.cpp)
target_include_directories(YesDawSumCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawSumCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawSumCheck)
catch_discover_tests(YesDawSumCheck)

# --- MeterNode check (H1; pure, peak/RMS tap with lock-free publish — ADR-0008) ---
add_executable(YesDawMeterCheck tests/meter_tests.cpp)
target_include_directories(YesDawMeterCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawMeterCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawMeterCheck)
catch_discover_tests(YesDawMeterCheck)

# --- Mixer projection check (H3; pure headless mixer-as-graph foundation) ---
add_executable(YesDawMixerProjectionCheck tests/mixer_projection_tests.cpp)
target_include_directories(YesDawMixerProjectionCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawMixerProjectionCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawMixerProjectionCheck)
catch_discover_tests(YesDawMixerProjectionCheck)

# --- Mixer mute policy check (H3; mute / SIP-solo / solo-safe -> post-compile mute mask, ADR-0014) ---
add_executable(YesDawMixerMutePolicyCheck tests/mixer_mute_policy_tests.cpp)
target_include_directories(YesDawMixerMutePolicyCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawMixerMutePolicyCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawMixerMutePolicyCheck)
catch_discover_tests(YesDawMixerMutePolicyCheck)

# --- Sidechain input pin check (H3; ordered aux inputs + PDC convergence, ADR-0014) ---
add_executable(YesDawSidechainCheck tests/sidechain_tests.cpp)
target_include_directories(YesDawSidechainCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawSidechainCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawSidechainCheck)
catch_discover_tests(YesDawSidechainCheck)

# --- RT-lane IPC ring check (H3; pure, the lock-free double-buffered audio+Event ring — ADR-0015) ---
# Built UNCONDITIONALLY (no JUCE) so the RTSan leg covers the audio-thread role (exchangeBlock) under
# -fsanitize=realtime and the TSan leg covers the producer/consumer release/acquire protocol under
# -fsanitize=thread. The free-running stress test needs the platform threads library.
add_executable(YesDawPluginIpcCheck tests/rt_lane_tests.cpp)
target_include_directories(YesDawPluginIpcCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawPluginIpcCheck PRIVATE Catch2::Catch2WithMain Threads::Threads)
yesdaw_harden(YesDawPluginIpcCheck)
catch_discover_tests(YesDawPluginIpcCheck)

# --- PluginNode IPC-proxy check (H3; the graph-visible Node adapter over the RT-lane ring — ADR-0015) ---
# Pure C++ (the "plugin" is an in-process stub child — NO JUCE hosting yet), built UNCONDITIONALLY so the
# RTSan leg covers PluginNode::process() (exchangeBlock on the audio thread) and the TSan leg covers it too.
# The stub child is pumped synchronously by the test, so no platform threads library is needed here.
add_executable(YesDawPluginNodeCheck tests/plugin_node_tests.cpp)
target_include_directories(YesDawPluginNodeCheck PRIVATE ${YESDAW_SRC_DIR})
target_link_libraries(YesDawPluginNodeCheck PRIVATE Catch2::Catch2WithMain)
yesdaw_harden(YesDawPluginNodeCheck)
catch_discover_tests(YesDawPluginNodeCheck)

# --- WAV load + scrub check (spike #1; links JUCE, so guarded + headless-but-CI-run) ---
# Decodes a committed fixture WAV and golden-diffs/scrubs it — no listening. Links juce_audio_formats
# (file decode only, no device/display), so it joins the normal matrix + ctest but is skipped on the
# pure-test RTSan leg.
if (YESDAW_BUILD_APPS)
  juce_add_console_app(YesDawAssetCheck PRODUCT_NAME "YesDawAssetCheck")
  target_sources(YesDawAssetCheck PRIVATE tests/wav_tests.cpp)
  target_compile_definitions(YesDawAssetCheck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    YESDAW_WAV_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/sine_440_48k_mono.wav")
  target_link_libraries(YesDawAssetCheck
    PRIVATE
      Catch2::Catch2WithMain
      juce::juce_audio_formats
      juce::juce_recommended_config_flags)
  yesdaw_harden(YesDawAssetCheck)
  catch_discover_tests(YesDawAssetCheck)

  juce_add_console_app(YesDawBundleRenderCheck PRODUCT_NAME "YesDawBundleRenderCheck")
  target_sources(YesDawBundleRenderCheck PRIVATE tests/bundle_render_tests.cpp)
  target_include_directories(YesDawBundleRenderCheck PRIVATE ${YESDAW_SRC_DIR})
  target_include_directories(YesDawBundleRenderCheck SYSTEM PRIVATE ${choc_SOURCE_DIR})
  target_compile_definitions(YesDawBundleRenderCheck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    YESDAW_WAV_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/sine_440_48k_mono.wav")
  target_link_libraries(YesDawBundleRenderCheck
    PRIVATE
      Catch2::Catch2WithMain
      sqlite3_amalgamation
      juce::juce_audio_formats
      juce::juce_recommended_config_flags)
  yesdaw_harden(YesDawBundleRenderCheck)
  catch_discover_tests(YesDawBundleRenderCheck)
endif()

# Bless the golden after an INTENTIONAL DSP change (review the diff before committing):
#   cmake --build --preset ci --target bless-goldens   # path-free; targets the preset's build-ci/
add_custom_target(bless-goldens
  COMMAND $<TARGET_FILE:YesDawCheck> --emit-golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/sine_440_48k.csv"
  DEPENDS YesDawCheck
  VERBATIM
  COMMENT "Regenerating goldens from the current DSP")

# --- Hosting layering check (H3; ADR-0008 / ADR-0015) ----------------------------
# The worker executable is the only target in this checkpoint that may link JUCE's plugin-hosting module.
# The engine remains header-only/pure-C++ today, so the enforceable boundary is the set of pure engine test
# targets that compile and exercise engine code in CI, RTSan, and TSan.
set(YESDAW_ENGINE_LAYER_TARGETS
  YesDawCheck
  YesDawUiCheck
  YesDawEngineCheck
  YesDawTimeCheck
  YesDawEventCheck
  YesDawProjectCheck
  YesDawPersistenceCheck
  YesDawGraphCheck
  YesDawBuilderCheck
  YesDawRuntimeCheck
  YesDawRenderCheck
  YesDawDelayCheck
  YesDawFaderCheck
  YesDawPanCheck
  YesDawSumCheck
  YesDawMeterCheck
  YesDawMixerProjectionCheck
  YesDawMixerMutePolicyCheck
  YesDawSidechainCheck
  YesDawPluginIpcCheck
  YesDawPluginNodeCheck)

set(_yesdaw_hosting_violations)
foreach(_target IN LISTS YESDAW_ENGINE_LAYER_TARGETS)
  if (NOT TARGET ${_target})
    continue()
  endif()

  get_target_property(_links ${_target} LINK_LIBRARIES)
  if (NOT _links)
    set(_links)
  endif()

  foreach(_link IN LISTS _links)
    if (_link MATCHES "juce::juce_audio_processors|juce_audio_processors|AudioPluginFormatManager")
      list(APPEND _yesdaw_hosting_violations "${_target} -> ${_link}")
    endif()
  endforeach()
endforeach()

if (_yesdaw_hosting_violations)
  list(JOIN _yesdaw_hosting_violations "\n  " _yesdaw_hosting_violations_text)
  message(FATAL_ERROR "Engine-side targets must not link JUCE plugin hosting:\n  ${_yesdaw_hosting_violations_text}")
endif()

if (YESDAW_BUILD_APPS)
  get_target_property(_plugin_host_links YesDawPluginHost LINK_LIBRARIES)
  if (NOT _plugin_host_links MATCHES "juce::juce_audio_processors|juce_audio_processors")
    message(FATAL_ERROR "YesDawPluginHost must own the JUCE plugin-hosting module")
  endif()
endif()
