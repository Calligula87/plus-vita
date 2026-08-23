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
if(NOT EXISTS "${PYLUX_ROOT}/lib/src/base64.c")
  message(FATAL_ERROR "Pylux submodule is missing; run: git submodule update --init --recursive")
endif()

add_executable(${PROJECT_NAME}
  src/main.c
  "${PYLUX_ROOT}/lib/src/base64.c"
)

target_include_directories(${PROJECT_NAME} PRIVATE
  "${PYLUX_ROOT}/lib/include"
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

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
