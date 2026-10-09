if(NOT DEFINED REPORT_PATH OR NOT EXISTS "${REPORT_PATH}")
  message(FATAL_ERROR "Timeline diagnostic report missing; run the GPU test with profiling enabled.")
endif()
file(READ "${REPORT_PATH}" report)
message("--- Timeline paint diagnostic report ---\n${report}\n--- End timeline paint diagnostic report ---")
if(NOT report MATCHES "diagnostic=timeline-paint-v1" OR NOT report MATCHES "sustained_frame_index="
   OR NOT report MATCHES "residual_ms=-?[0-9]+[.][0-9]+")
  message(FATAL_ERROR "Timeline diagnostic report has no complete stage measurement.")
endif()
