# CMake toolchain file: cross-compile for PlayStation 4 with the OpenOrbis
# toolchain (LLVM 18 release, FreeBSD 12 ABI, static musl libc + libc++).
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=ps4/toolchain/ps4-kodi.cmake -S <src> -B <build>
#
# Environment: OO_PS4_TOOLCHAIN = root of the unpacked toolchain
# (the folder that contains bin/ include/ lib/ link.x).

if(NOT DEFINED OO_PS4_TOOLCHAIN)
  if(DEFINED ENV{OO_PS4_TOOLCHAIN})
    set(OO_PS4_TOOLCHAIN "$ENV{OO_PS4_TOOLCHAIN}")
  else()
    set(OO_PS4_TOOLCHAIN "/opt/OpenOrbis/PS4Toolchain")
  endif()
endif()
if(NOT EXISTS "${OO_PS4_TOOLCHAIN}/link.x")
  message(FATAL_ERROR "OpenOrbis toolchain not found at ${OO_PS4_TOOLCHAIN} (set OO_PS4_TOOLCHAIN)")
endif()

set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_SYSTEM_VERSION 12)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(PS4 TRUE)

set(PS4_TRIPLE x86_64-pc-freebsd12-elf)
find_program(PS4_CLANG   NAMES clang-18 clang   REQUIRED)
find_program(PS4_CLANGXX NAMES clang++-18 clang++ REQUIRED)
set(CMAKE_C_COMPILER   "${PS4_CLANG}")
set(CMAKE_CXX_COMPILER "${PS4_CLANGXX}")
set(CMAKE_C_COMPILER_TARGET   ${PS4_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${PS4_TRIPLE})
set(CMAKE_ASM_COMPILER_TARGET ${PS4_TRIPLE})
find_program(CMAKE_AR     NAMES llvm-ar-18 llvm-ar       REQUIRED)
find_program(CMAKE_RANLIB NAMES llvm-ranlib-18 llvm-ranlib REQUIRED)
find_program(CMAKE_STRIP  NAMES llvm-strip-18 llvm-strip)
find_program(CMAKE_NM     NAMES llvm-nm-18 llvm-nm)

# A sysroot-like prefix for libraries we cross-build ourselves (FFmpeg, ...).
set(PS4_PREFIX "${OO_PS4_TOOLCHAIN}/ps4mc-prefix" CACHE PATH "install prefix of cross-built libs")

set(_ps4_common "-fPIC -funwind-tables -D_GNU_SOURCE -D__PS4__=1 -isysroot ${OO_PS4_TOOLCHAIN} -isystem ${OO_PS4_TOOLCHAIN}/include -isystem ${PS4_PREFIX}/include")
set(CMAKE_C_FLAGS_INIT   "${_ps4_common}")
set(CMAKE_CXX_FLAGS_INIT "${_ps4_common} -isystem ${OO_PS4_TOOLCHAIN}/include/c++/v1 -fexceptions -fcxx-exceptions")

# Executables: PIE, Sony linker script, our libc, Sony stub libraries.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64 -Wl,--script,${OO_PS4_TOOLCHAIN}/link.x -Wl,--eh-frame-hdr -L${OO_PS4_TOOLCHAIN}/lib -L${PS4_PREFIX}/lib")
set(CMAKE_C_STANDARD_LIBRARIES   "-lc -lkernel -lSceSysmodule ${OO_PS4_TOOLCHAIN}/lib/crt1.o")
set(CMAKE_CXX_STANDARD_LIBRARIES "-lc++ -lc -lkernel -lSceSysmodule ${OO_PS4_TOOLCHAIN}/lib/crt1.o")

# No shared libraries: a title cannot dlopen anything we ship.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

list(APPEND CMAKE_FIND_ROOT_PATH "${PS4_PREFIX}" "${OO_PS4_TOOLCHAIN}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_LIBDIR}      "${PS4_PREFIX}/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")

# Kodi platform selection (the 'ps4' platform is added by ps4/overlay).
set(CORE_SYSTEM_NAME   ps4   CACHE STRING "" FORCE)
set(CORE_PLATFORM_NAME ps4   CACHE STRING "" FORCE)
set(APP_RENDER_SYSTEM  gles  CACHE STRING "" FORCE)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
