cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to the VitaSDK installation directory")
  endif()
endif()

project(plus_vita_smoke C)

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "plus-vita smoke")
set(VITA_TITLEID "PLVT00001")
set(VITA_VERSION "00.01")
set(PLUS_VITA_TLS_PROBE_URL "" CACHE STRING
  "Optional controlled HTTPS URL for the physical certificate-rejection test")
set(PLUS_VITA_TLS_TRUSTED_CA_FILE "" CACHE FILEPATH
  "Optional DER test CA packaged for the controlled trusted-HTTPS test")
set(PLUS_VITA_PUBLIC_HTTPS_URL "" CACHE STRING
  "Optional credential-free public HTTPS URL for a system-trust test")
set(PLUS_VITA_POST_ECHO_URL "" CACHE STRING
  "Optional controlled LAN URL for a physical POST echo test")

set(PYLUX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../external/pylux")
set(NANOPB_ROOT "${PYLUX_ROOT}/third-party/nanopb")
set(JSONC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../external/json-c")
set(VITASDK_COMMON "$ENV{VITASDK}/share/gcc-arm-vita-eabi/samples/common")
if(NOT EXISTS "${PYLUX_ROOT}/lib/src/base64.c")
  message(FATAL_ERROR "Pylux submodule is missing; run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${NANOPB_ROOT}/pb_encode.c")
  message(FATAL_ERROR "Pylux nanopb submodule is missing; run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${JSONC_ROOT}/json_object.c")
  message(FATAL_ERROR "json-c submodule is missing; run: git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(DISABLE_WERROR ON CACHE BOOL "" FORCE)
set(DISABLE_THREAD_LOCAL_STORAGE ON CACHE BOOL "" FORCE)
set(DISABLE_EXTRA_LIBS ON CACHE BOOL "" FORCE)
set(DISABLE_STATIC_FPIC ON CACHE BOOL "" FORCE)
# VitaSDK/newlib exposes these, but json-c only probes several of them when
# CMake's target platform is classified as UNIX. The Vita toolchain is Generic.
set(HAVE_SNPRINTF 1 CACHE INTERNAL "VitaSDK provides snprintf" FORCE)
set(HAVE_VASPRINTF 1 CACHE INTERNAL "VitaSDK provides vasprintf" FORCE)
set(HAVE_DECL_INFINITY 1 CACHE INTERNAL "VitaSDK provides INFINITY" FORCE)
set(HAVE_DECL_ISINF 1 CACHE INTERNAL "VitaSDK provides isinf" FORCE)
set(HAVE_DECL_ISNAN 1 CACHE INTERNAL "VitaSDK provides isnan" FORCE)
set(HAVE_DECL_NAN 1 CACHE INTERNAL "VitaSDK provides NAN" FORCE)
add_subdirectory("${JSONC_ROOT}" "${CMAKE_BINARY_DIR}/json-c" EXCLUDE_FROM_ALL)

add_executable(${PROJECT_NAME}
  src/main.c
  generated/takion.pb.c
  "${PYLUX_ROOT}/lib/src/base64.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_consts.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_cache.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_fetch.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_merge.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_util.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_unified.c"
  "${PYLUX_ROOT}/lib/src/log.c"
  "${PYLUX_ROOT}/lib/src/sock.c"
  "${PYLUX_ROOT}/lib/src/thread.c"
  "${NANOPB_ROOT}/pb_common.c"
  "${NANOPB_ROOT}/pb_decode.c"
  "${NANOPB_ROOT}/pb_encode.c"
  src/platform/random.c
  src/platform/catalog_fetch_guard.c
  src/platform/curl_escape_compat.c
  src/platform/network.c
  src/platform/secure_http.c
  src/platform/thread_compat.c
  src/platform/time.c
  src/platform/vita_cc_http.c
  "${VITASDK_COMMON}/debugScreen.c"
)

# Pylux currently has one signed/unsigned comparison in its formatting path.
# Keep -Werror for our code and suppress only that upstream diagnostic.
set_source_files_properties("${PYLUX_ROOT}/lib/src/log.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=sign-compare"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/cloudcatalog_merge.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=format-truncation"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/cloudcatalog_fetch.c" PROPERTIES
  COMPILE_DEFINITIONS
    "cc_fetch_psnow_native=plus_vita_disabled_psnow_native_impl;cc_fetch_imagic=plus_vita_disabled_imagic_impl;cc_imagic_result_fini=plus_vita_disabled_imagic_fini_impl;cc_fetch_owned=plus_vita_disabled_owned_impl"
  COMPILE_OPTIONS "-Wno-error=unused-but-set-variable"
)

# This pure catalog module does not use json-c. Its private umbrella header
# includes json-c for sibling modules, so skip that header body for this source.
set_source_files_properties("${PYLUX_ROOT}/lib/src/cloudcatalog_consts.c" PROPERTIES
  COMPILE_DEFINITIONS "CHIAKI_CLOUDCATALOG_INTERNAL_H=1"
)

# Supply the Vita-only declaration/redirect for GNU pthread_clockjoin_np.
set_source_files_properties("${PYLUX_ROOT}/lib/src/thread.c" PROPERTIES
  COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/src/platform/thread_compat.h"
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/compat"
	"${CMAKE_CURRENT_LIST_DIR}/../external"
  "${PYLUX_ROOT}/lib/include"
  "${PYLUX_ROOT}/lib/src"
  "${NANOPB_ROOT}"
  "${CMAKE_CURRENT_LIST_DIR}/generated"
  "${VITASDK_COMMON}"
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall
  -Wextra
  -Werror
  -ffunction-sections
  -fdata-sections
)

if(PLUS_VITA_TLS_PROBE_URL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUS_VITA_TLS_PROBE_URL="${PLUS_VITA_TLS_PROBE_URL}"
  )
endif()

set(PLUS_VITA_VPK_FILES)
if(PLUS_VITA_TLS_TRUSTED_CA_FILE)
  if(NOT EXISTS "${PLUS_VITA_TLS_TRUSTED_CA_FILE}")
    message(FATAL_ERROR "Trusted test CA does not exist: ${PLUS_VITA_TLS_TRUSTED_CA_FILE}")
  endif()
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLUS_VITA_TLS_TRUSTED_PROBE=1)
  list(APPEND PLUS_VITA_VPK_FILES FILE
    "${PLUS_VITA_TLS_TRUSTED_CA_FILE}" "local_probe_ca.der")
endif()
if(PLUS_VITA_PUBLIC_HTTPS_URL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUS_VITA_PUBLIC_HTTPS_URL="${PLUS_VITA_PUBLIC_HTTPS_URL}"
  )
endif()
if(PLUS_VITA_POST_ECHO_URL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUS_VITA_POST_ECHO_URL="${PLUS_VITA_POST_ECHO_URL}"
  )
endif()

target_link_options(${PROJECT_NAME} PRIVATE
  -Wl,--gc-sections
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	json-c
  pthread
  SceLibKernel_stub
  SceCtrl_stub
  SceDisplay_stub
  SceKernelThreadMgr_stub
  SceSysmem_stub
  SceNet_stub
  SceNetCtl_stub
  SceHttp_stub
  SceSsl_stub
  SceSysmodule_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  ${PLUS_VITA_VPK_FILES}
)
