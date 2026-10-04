cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
  else()
    message(FATAL_ERROR "VITASDK n'est pas defini. Exporte VITASDK avant de lancer CMake.")
  endif()
endif()

project(kh1vita_test C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -O2")

set(VITA_APP_NAME "KH1 Vita Sora Test")
set(VITA_TITLEID "KH1VITA01")
set(VITA_VERSION "00.05")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")

add_executable(kh1vita
  src/main.c
  src/platform_vita.c
  src/renderer_vita.c
  src/payload.c
  src/kh1_ard.c
  src/kh1_mdls.c
  src/kh1_mdls_geometry.c
  src/kh1_mdls_skeleton.c
  src/kh1_mdls_texture.c
  src/kh1_mset.c
  src/software_rasterizer.c
  src/game_bridge.c
)

target_include_directories(kh1vita PRIVATE include)

target_link_libraries(kh1vita
  m
  SceCtrl_stub
  SceDisplay_stub
  SceLibKernel_stub
  SceIofilemgr_stub
)

vita_create_self(eboot.bin kh1vita)
vita_create_vpk(KH1Vita_Test.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
