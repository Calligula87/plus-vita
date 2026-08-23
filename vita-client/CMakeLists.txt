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

set(PYLUX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../external/pylux")
set(NANOPB_ROOT "${PYLUX_ROOT}/third-party/nanopb")
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
  "${PYLUX_ROOT}/lib/src/thread.c"
  "${NANOPB_ROOT}/pb_common.c"
  "${NANOPB_ROOT}/pb_decode.c"
  "${NANOPB_ROOT}/pb_encode.c"
  src/platform/random.c
  src/platform/thread_compat.c
  src/platform/time.c
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
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall
  -Wextra
  -Werror
  -ffunction-sections
  -fdata-sections
)

target_link_options(${PROJECT_NAME} PRIVATE
  -Wl,--gc-sections
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  pthread
  SceLibKernel_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
