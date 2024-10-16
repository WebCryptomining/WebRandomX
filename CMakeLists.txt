# Copyright (c) 2019, The Monero Project
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
# 
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.10.2)

project(WebRandomX)

set(randomx_sources
src/cpp/aes_hash.cpp
src/cpp/argon2_simd.c
src/cpp/bytecode_machine.cpp
src/cpp/dataset.cpp
src/cpp/virtual_memory.cpp
src/cpp/vm_interpreted.cpp
src/cpp/allocator.cpp
src/cpp/assembly_generator_x86.cpp
src/cpp/instruction.cpp
src/cpp/randomx.cpp
src/cpp/superscalar.cpp
src/cpp/vm_compiled.cpp
src/cpp/vm_interpreted_light.cpp
src/cpp/argon2_core.c
src/cpp/blake2_generator.cpp
src/cpp/intrin_wasm.cpp
src/cpp/softfloat.cpp
src/cpp/reciprocal.c
src/cpp/virtual_machine.cpp
src/cpp/vm_compiled_light.cpp
src/cpp/blake2b.c)

if(NOT ARCH_ID)
  # allow cross compiling
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "")
    set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
  endif()
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH_ID)
endif()

if(NOT ARM_ID)
  set(ARM_ID "${ARCH_ID}")
endif()

if(NOT ARCH)
  set(ARCH "default")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "Setting default build type: ${CMAKE_BUILD_TYPE}")
endif()

include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

function(add_flag flag)
  string(REPLACE "-" "_" supported_cxx ${flag}_cxx)
  check_cxx_compiler_flag(${flag} ${supported_cxx})
  if(${${supported_cxx}})
    message(STATUS "Setting CXX flag ${flag}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
  endif()
  string(REPLACE "-" "_" supported_c ${flag}_c)
  check_c_compiler_flag(${flag} ${supported_c})
  if(${${supported_c}})
    message(STATUS "Setting C flag ${flag}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
  endif()
endfunction()

# x86-64
if(ARCH_ID STREQUAL "x86_64" OR ARCH_ID STREQUAL "x86-64" OR ARCH_ID STREQUAL "amd64")
  if(MSVC)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/asm/configuration.asm
      COMMAND powershell -ExecutionPolicy Bypass -File h2inc.ps1 ..\\src\\configuration.h > ..\\src\\asm\\configuration.asm SET ERRORLEVEL = 0
      COMMENT "Generating configuration.asm at ${CMAKE_CURRENT_SOURCE_DIR}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vcxproj)
    add_custom_target(generate-asm
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/asm/configuration.asm)
  else()
    if(ARCH STREQUAL "native")
      add_flag("-march=native")
    endif()
  endif()
endif()

# PowerPC
if(ARCH_ID STREQUAL "ppc64" OR ARCH_ID STREQUAL "ppc64le")
  if(ARCH STREQUAL "native")
    add_flag("-mcpu=native")
  endif()
  # PowerPC AES requires ALTIVEC (POWER7+), so it cannot be enabled in the default build
endif()

# ARMv8
if(ARM_ID STREQUAL "aarch64" OR ARM_ID STREQUAL "arm64" OR ARM_ID STREQUAL "armv8-a")
  # not sure if this check is needed
  include(CheckIncludeFile)
  check_include_file(asm/hwcap.h HAVE_HWCAP)
  if(HAVE_HWCAP)
    add_definitions(-DHAVE_HWCAP)
  endif()

  if(ARCH STREQUAL "native")
    add_flag("-march=native")
  endif()
endif()

# WASM SIMD
set_source_files_properties(${randomx_sources} COMPILE_FLAGS -msimd128)

set(RANDOMX_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src" CACHE STRING "WebRandomX Include path")

add_library(randomx ${randomx_sources})

if(TARGET generate-asm)
  add_dependencies(randomx generate-asm)
endif()

set_property(TARGET randomx PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET randomx PROPERTY CXX_STANDARD 11)
set_property(TARGET randomx PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET randomx PROPERTY PUBLIC_HEADER src/cpp/randomx.h)

include(GNUInstallDirs)
install(TARGETS randomx
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 
# Add executables
# 

if(NOT Threads_FOUND AND UNIX AND NOT APPLE)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads)
endif()

add_executable(web-randomx
  src/cpp/web_randomx.cpp)
target_link_libraries(web-randomx
  PRIVATE randomx)
set_property(TARGET web-randomx PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET web-randomx PROPERTY CXX_STANDARD 11)
set_target_properties(web-randomx PROPERTIES LINK_FLAGS "-os -s WASM=1 -s MODULARIZE=1 -s WASM_BIGINT -s TOTAL_MEMORY=272MB -msimd128 -s EXPORTED_FUNCTIONS=\"['_free', '_malloc']\"")

# Tests
if(TESTS AND TESTS STREQUAL "true")
  set_source_files_properties(src/cpp/tests/tests.cpp COMPILE_FLAGS -msimd128)
  add_executable(web-randomx-tests
    src/cpp/tests/tests.cpp)
  target_link_libraries(web-randomx-tests
    PRIVATE randomx)
  set_property(TARGET web-randomx-tests PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_property(TARGET web-randomx-tests PROPERTY CXX_STANDARD 11)
  set_target_properties(web-randomx-tests PROPERTIES LINK_FLAGS "-g3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -msimd128")

  set_source_files_properties(src/cpp/tests/benchmark.cpp COMPILE_FLAGS -msimd128)
  add_executable(web-randomx-benchmark
    src/cpp/tests/benchmark.cpp)
  target_link_libraries(web-randomx-benchmark
    PRIVATE randomx)
  set_property(TARGET web-randomx-benchmark PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_property(TARGET web-randomx-benchmark PROPERTY CXX_STANDARD 11)
  set_target_properties(web-randomx-benchmark PROPERTIES LINK_FLAGS "-os -s WASM=1 -s MAXIMUM_MEMORY=4GB -s ALLOW_MEMORY_GROWTH=1 -msimd128")
endif()
