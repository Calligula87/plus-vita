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

set(PYLUX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../external/pylux")
set(NANOPB_ROOT "${PYLUX_ROOT}/third-party/nanopb")
set(VITASDK_COMMON "$ENV{VITASDK}/share/gcc-arm-vita-eabi/samples/common")
if(NOT EXISTS "${PYLUX_ROOT}/lib/src/base64.c")
  message(FATAL_ERROR "Pylux submodule is missing; run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${NANOPB_ROOT}/pb_encode.c")
  message(FATAL_ERROR "Pylux nanopb submodule is missing; run: git submodule update --init --recursive")
endif()

add_executable(${PROJECT_NAME}
  src/main.c
  generated/takion.pb.c
  "${PYLUX_ROOT}/lib/src/base64.c"
  "${PYLUX_ROOT}/lib/src/log.c"
  "${PYLUX_ROOT}/lib/src/sock.c"
  "${PYLUX_ROOT}/lib/src/thread.c"
  "${NANOPB_ROOT}/pb_common.c"
  "${NANOPB_ROOT}/pb_decode.c"
  "${NANOPB_ROOT}/pb_encode.c"
  src/platform/random.c
  src/platform/network.c
  src/platform/secure_http.c
  src/platform/thread_compat.c
  src/platform/time.c
  "${VITASDK_COMMON}/debugScreen.c"
)

# Pylux currently has one signed/unsigned comparison in its formatting path.
# Keep -Werror for our code and suppress only that upstream diagnostic.
set_source_files_properties("${PYLUX_ROOT}/lib/src/log.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=sign-compare"
)

# Supply the Vita-only declaration/redirect for GNU pthread_clockjoin_np.
set_source_files_properties("${PYLUX_ROOT}/lib/src/thread.c" PROPERTIES
  COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/src/platform/thread_compat.h"
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${PYLUX_ROOT}/lib/include"
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

target_link_options(${PROJECT_NAME} PRIVATE
  -Wl,--gc-sections
)

target_link_libraries(${PROJECT_NAME} PRIVATE
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
