# Cross-compile LLVM libc++ for the OpenOrbis PS4 sysroot.
if(NOT DEFINED OO_PS4_TOOLCHAIN)
  if(DEFINED ENV{OO_PS4_TOOLCHAIN})
    set(OO_PS4_TOOLCHAIN "$ENV{OO_PS4_TOOLCHAIN}")
  else()
    message(FATAL_ERROR "OO_PS4_TOOLCHAIN must point to the OpenOrbis SDK")
  endif()
endif()
if(NOT DEFINED PS4_PREFIX)
  get_filename_component(PS4_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../depends/prefix" ABSOLUTE)
  set(PS4_PREFIX "${PS4_PREFIX}" CACHE PATH "PS4 dependency prefix")
endif()

set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_SYSTEM_VERSION 12)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(PS4_TRIPLE x86_64-pc-freebsd12-elf)
set(CMAKE_C_COMPILER /usr/bin/clang-18 CACHE FILEPATH "PS4 C compiler")
set(CMAKE_CXX_COMPILER /usr/bin/clang++-18 CACHE FILEPATH "PS4 C++ compiler")
set(CMAKE_C_COMPILER_TARGET ${PS4_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${PS4_TRIPLE})
set(CMAKE_SYSROOT "${OO_PS4_TOOLCHAIN}")

execute_process(
  COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
  OUTPUT_VARIABLE PS4_CLANG_RESOURCE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY)

set(_ps4_sysroot_flags
    "-nostdinc -fPIC -funwind-tables -D_GNU_SOURCE -D__PS4__=1 -isysroot ${OO_PS4_TOOLCHAIN} -isystem ${OO_PS4_TOOLCHAIN}/include -isystem ${PS4_CLANG_RESOURCE_DIR}/include -isystem ${PS4_PREFIX}/include")
set(CMAKE_C_FLAGS_INIT "${_ps4_sysroot_flags}")
set(CMAKE_CXX_FLAGS_INIT
    "${_ps4_sysroot_flags} -isystem ${CMAKE_CURRENT_LIST_DIR}/../build/llvm-project/libcxxabi/include -D_LIBCPP_HAS_MUSL_LIBC -D_LIBCPP_DISABLE_AVAILABILITY -fexceptions -fcxx-exceptions")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-nostdlib -fuse-ld=lld -L${PS4_PREFIX}/lib -L${OO_PS4_TOOLCHAIN}/lib")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES OO_PS4_TOOLCHAIN PS4_PREFIX)

find_program(CMAKE_AR NAMES llvm-ar-18 llvm-ar REQUIRED)
find_program(CMAKE_RANLIB NAMES llvm-ranlib-18 llvm-ranlib REQUIRED)
