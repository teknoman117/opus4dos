# SPDX-License-Identifier: BSD-3-Clause
# OPUS4DOS: an Ogg Opus player for DOS, on libopus (external/opus, a submodule, unmodified).
#
#   . ~/bin/watcom/owsetenv.sh
#   cmake --preset dos && cmake --build --preset dos      -> build/dos/OPUS4DOS.EXE
cmake_minimum_required(VERSION 3.21)
# The dos preset (CMakePresets.json) makes this a 32-bit DOS build with wcl386, which needs
# OpenWatcom's environment: owsetenv.sh sets it up.
if(NOT DEFINED ENV{WATCOM} OR "$ENV{WATCOM}" STREQUAL "")
    message(FATAL_ERROR "WATCOM isn't set: source OpenWatcom's owsetenv.sh first")
endif()
project(opus4dos C)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "OpenWatcom")
    message(FATAL_ERROR "OPUS4DOS is built by OpenWatcom: source owsetenv.sh and use `cmake --preset dos`")
endif()

# One set of flags whatever the build type: the flat model (-mf), 386 register calling (-3r),
# floating point by library calls so no FPU is needed (-fpc), no stack checks (-s), optimized
# for speed (-otexan), structures packed to 4 bytes (-zp4). DOS/32A is bound in as the stub.
set(CMAKE_C_FLAGS "-bt=dos")
foreach(type DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_C_FLAGS_${type} "")
    set(CMAKE_EXE_LINKER_FLAGS_${type} "")
endforeach()
# owsetenv.sh doesn't put binw, where the stub is, on the PATH, so wlink is told where it is.
set(CMAKE_EXE_LINKER_FLAGS "system dos32a option stub=$ENV{WATCOM}/binw/dos32a.exe")
add_compile_options(-mf -3r -fpc -s -otexan -zp4)

# libopus, by its own CMake build: a fixed-point decoder (no FPU needed), without the float API,
# x86 intrinsics, programs, tests or install rules.
set(OPUS_FIXED_POINT ON CACHE BOOL "" FORCE)
set(OPUS_ENABLE_FLOAT_API OFF CACHE BOOL "" FORCE)
set(OPUS_DISABLE_INTRINSICS ON CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(external/opus EXCLUDE_FROM_ALL)

add_executable(opus4dos
    src/opus4dos.c src/ogg.c src/resample.c
    src/audio.c src/sb16.c src/isadma.c src/irq.c src/dosmem.c)
target_compile_options(opus4dos PRIVATE -w3 -we)
target_link_libraries(opus4dos PRIVATE opus)
set_target_properties(opus4dos PROPERTIES OUTPUT_NAME OPUS4DOS SUFFIX .EXE LINK_FLAGS "option stack=64k")
