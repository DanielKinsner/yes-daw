cmake_minimum_required(VERSION 3.25)

# A cache startup failure must still run the real compiler and every build/test gate.
execute_process(COMMAND sccache --show-stats
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
