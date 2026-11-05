cmake_minimum_required(VERSION 3.25)

# Stats can succeed without a server. CI selects startup; retain the old query
# policy for existing callers and its frozen regression.
if(NOT DEFINED YESDAW_CI_CACHE_PROBE)
  set(YESDAW_CI_CACHE_PROBE legacy-stats)
endif()
if(YESDAW_CI_CACHE_PROBE STREQUAL "startup")
  set(cache_probe --start-server)
elseif(YESDAW_CI_CACHE_PROBE STREQUAL "legacy-stats")
  set(cache_probe --show-stats)
else()
  message(FATAL_ERROR "Unknown CI cache probe policy")
endif()
execute_process(COMMAND sccache "${cache_probe}"
  RESULT_VARIABLE cache_status OUTPUT_QUIET ERROR_QUIET TIMEOUT 30)
set(compiler_launcher "")
set(cache_enabled false)
if(cache_status STREQUAL "0")
  set(compiler_launcher sccache)
  set(cache_enabled true)
else()
  message(STATUS "Compiler cache unavailable (${cache_status}); compiling directly")
endif()

if(DEFINED ENV{GITHUB_OUTPUT})
  file(APPEND "$ENV{GITHUB_OUTPUT}" "cache_enabled=${cache_enabled}\n")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" --preset ci
  "-DCMAKE_C_COMPILER_LAUNCHER=${compiler_launcher}"
  "-DCMAKE_CXX_COMPILER_LAUNCHER=${compiler_launcher}"
  RESULT_VARIABLE configure_status COMMAND_ECHO STDOUT)
if(NOT configure_status STREQUAL "0")
  message(FATAL_ERROR "CI configuration failed: ${configure_status}")
endif()
