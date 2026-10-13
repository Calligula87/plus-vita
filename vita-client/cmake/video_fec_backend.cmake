# Scalar builds of the same GF-Complete/Jerasure backend used by Pylux.
# Do not enable platform SIMD until reconstruction is measured on hardware.
set(PLUS_VITA_GF_ROOT "${PYLUX_ROOT}/third-party/gf-complete")
set(PLUS_VITA_JERASURE_ROOT "${PYLUX_ROOT}/third-party/jerasure")
if(NOT EXISTS "${PLUS_VITA_GF_ROOT}/src/gf.c"
    OR NOT EXISTS "${PLUS_VITA_JERASURE_ROOT}/src/jerasure.c")
  message(FATAL_ERROR "Pylux GF-Complete/Jerasure submodules are missing")
endif()
add_library(plus_vita_gf_complete STATIC
  "${PLUS_VITA_GF_ROOT}/src/gf.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_wgen.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_w4.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_w8.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_w16.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_w32.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_w64.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_w128.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_rand.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_general.c"
  "${PLUS_VITA_GF_ROOT}/src/gf_cpu.c")
target_include_directories(plus_vita_gf_complete PUBLIC "${PLUS_VITA_GF_ROOT}/include")
add_library(plus_vita_jerasure STATIC
  "${PLUS_VITA_JERASURE_ROOT}/src/galois.c"
  "${PLUS_VITA_JERASURE_ROOT}/src/jerasure.c"
  "${PLUS_VITA_JERASURE_ROOT}/src/reed_sol.c"
  "${PLUS_VITA_JERASURE_ROOT}/src/cauchy.c"
  "${PLUS_VITA_JERASURE_ROOT}/src/liberation.c")
target_include_directories(plus_vita_jerasure PUBLIC "${PLUS_VITA_JERASURE_ROOT}/include")
target_link_libraries(plus_vita_jerasure PUBLIC plus_vita_gf_complete)
foreach(fec_target plus_vita_gf_complete plus_vita_jerasure)
  target_compile_options(${fec_target} PRIVATE -ffunction-sections -fdata-sections)
endforeach()
