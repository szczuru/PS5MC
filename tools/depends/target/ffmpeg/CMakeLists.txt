project(ffmpeg)

cmake_minimum_required(VERSION 3.12)

set(ffmpeg_cc "${CMAKE_C_COMPILER}")
set(ffmpeg_cxx "${CMAKE_CXX_COMPILER}")
if(CMAKE_C_COMPILER_TARGET)
  string(APPEND ffmpeg_cc " --target=${CMAKE_C_COMPILER_TARGET}")
endif()
if(CMAKE_CXX_COMPILER_TARGET)
  string(APPEND ffmpeg_cxx " --target=${CMAKE_CXX_COMPILER_TARGET}")
endif()

if(ENABLE_CCACHE AND CCACHE_PROGRAM)
  set(ffmpeg_cc "${CCACHE_PROGRAM} ${ffmpeg_cc}")
  set(ffmpeg_cxx "${CCACHE_PROGRAM} ${ffmpeg_cxx}")
endif()

set(ffmpeg_conf "--cc=${ffmpeg_cc}" "--cxx=${ffmpeg_cxx}")

set(env_vars ${EXTRA_ENV_VARS})

if(CROSSCOMPILING)
  list(APPEND env_vars "PKG_CONFIG_LIBDIR=${DEPENDS_PATH}/lib/pkgconfig")
  list(APPEND ffmpeg_conf --pkg-config=${PKG_CONFIG_EXECUTABLE}
                          --pkg-config-flags=--static
                          --enable-cross-compile
                          --enable-pic
                          --ar=${CMAKE_AR}
                          --ranlib=${CMAKE_RANLIB}
                          --strip=${CMAKE_STRIP}
                          --host-cc=${CC_FOR_BUILD}
                          --host-ld=${CC_FOR_BUILD}
                          "--host-cflags=${CFLAGS_FOR_BUILD}"
                          "--host-ldflags=${LDFLAGS_FOR_BUILD}"
              )

  message(STATUS "CROSS: ${ffmpeg_conf}")
endif()

set(CONFIGARCH --arch=${CPU})

list(APPEND ffmpeg_conf --disable-doc
                        --disable-devices
                        --disable-programs
                        --disable-sdl2
                        --disable-vulkan
                        --enable-gpl
                        --enable-version3
                        --enable-openssl
                        --enable-runtime-cpudetect
                        --enable-pthreads
                        --extra-version="Kodi"
            )

if(CMAKE_C_FLAGS)
  list(APPEND ffmpeg_conf --extra-cflags=${CMAKE_C_FLAGS})
endif()

if(CMAKE_CXX_FLAGS)
  list(APPEND ffmpeg_conf --extra-cxxflags=${CMAKE_CXX_FLAGS})
endif()

if(CMAKE_EXE_LINKER_FLAGS)
  list(APPEND ffmpeg_conf --extra-ldflags=${CMAKE_EXE_LINKER_FLAGS})
endif()

# OpenOrbis links with -nostdlib. FFmpeg's configure link checks (including
# OpenSSL detection) therefore need the PS4 runtime libraries as trailing libs.
if(CORE_SYSTEM_NAME STREQUAL ps4)
  list(APPEND ffmpeg_conf "--extra-libs=-lkernel -lc -lm")
endif()

if(ENABLE_NEON)
  list(APPEND ffmpeg_conf --enable-neon)
endif()

if(CMAKE_BUILD_TYPE STREQUAL Release)
  list(APPEND ffmpeg_conf --disable-debug)
endif()

if(CORE_SYSTEM_NAME STREQUAL linux OR CORE_SYSTEM_NAME STREQUAL freebsd)
  list(APPEND ffmpeg_conf --enable-pic
                          --target-os=linux
              )
  if(ENABLE_VAAPI)
    list(APPEND ffmpeg_conf --enable-vaapi)
  else()
    list(APPEND ffmpeg_conf --disable-vaapi)
  endif()
  if(ENABLE_VDPAU)
    list(APPEND ffmpeg_conf --enable-vdpau)
  else()
    list(APPEND ffmpeg_conf --disable-vdpau)
  endif()
elseif(CORE_SYSTEM_NAME STREQUAL android)
  list(APPEND ffmpeg_conf --target-os=android
                          --extra-libs=-liconv
                          --disable-linux-perf
              )
  if(CPU MATCHES arm64)
    set(CONFIGARCH --arch=aarch64)
    list(APPEND ffmpeg_conf --cpu=cortex-a53)
  elseif(CPU MATCHES arm)
    list(APPEND ffmpeg_conf --cpu=cortex-a9)
  elseif(CPU MATCHES x86_64)
    list(APPEND ffmpeg_conf --cpu=x86-64)
    list(APPEND ffmpeg_conf --extra-cflags=-mno-stackrealign)
  else()
    list(APPEND ffmpeg_conf --cpu=i686 --disable-mmx --disable-asm)
    list(APPEND ffmpeg_conf --extra-cflags=-mno-stackrealign)
  endif()
elseif(CORE_SYSTEM_NAME STREQUAL darwin_embedded)
  list(APPEND ffmpeg_conf --enable-videotoolbox
                          --disable-filter=yadif_videotoolbox
                          --target-os=darwin
              )
elseif(CORE_SYSTEM_NAME STREQUAL osx)
  list(APPEND ffmpeg_conf --enable-videotoolbox
                          --target-os=darwin
                          --disable-securetransport
              )
elseif(CORE_SYSTEM_NAME STREQUAL wasm)
  list(APPEND ffmpeg_conf --target-os=none)
  set(CONFIGARCH --arch=wasm --cpu=wasm32)
elseif(CORE_SYSTEM_NAME STREQUAL ps4)
  # OpenOrbis is FreeBSD ABI; avoid host linux assumptions and asm that needs full toolchain.
  list(APPEND ffmpeg_conf --target-os=freebsd
                          --disable-asm
                          --disable-x86asm
                          --disable-inline-asm
                          --disable-vulkan
                          --disable-vaapi
                          --disable-vdpau
              )
endif()

if(CPU MATCHES arm)
  list(APPEND ffmpeg_conf --disable-armv5te --disable-armv6t2)
elseif(CPU MATCHES mips)
  list(APPEND ffmpeg_conf --disable-mips32r2 --disable-mipsdsp --disable-mipsdspr2)
endif()

if(CPU MATCHES x86 OR CPU MATCHES x86_64)
  find_package(NASM REQUIRED)
  list(APPEND ffmpeg_conf --x86asmexe=${NASM_EXECUTABLE})
endif()

if(USE_LTO)
   list(APPEND ffmpeg_conf --enable-lto)
endif()

if(ENABLE_DAV1D)
  list(APPEND ffmpeg_conf --enable-libdav1d)
  list(APPEND env_vars "PKG_CONFIG_PATH=${PKG_CONFIG_PATH}")
else()
  list(APPEND ffmpeg_conf --disable-libdav1d)
endif()

if(EXTRA_FLAGS)
  string(REPLACE " " ";" EXTRA_FLAGS ${EXTRA_FLAGS})
  list(APPEND ffmpeg_conf ${EXTRA_FLAGS})
endif()

list(APPEND ffmpeg_conf ${CONFIGARCH})

message(STATUS "FFMPEG_CONF: ${ffmpeg_conf}")

set(MAKE_COMMAND $(MAKE))
if(CMAKE_GENERATOR STREQUAL Ninja)
  set(MAKE_COMMAND make)
  include(ProcessorCount)
  ProcessorCount(N)
  if(NOT N EQUAL 0)
    set(MAKE_COMMAND make -j${N})
  endif()
endif()

if(NOT DISABLE_FFMPEG_SOURCE_PLUGINS)
  set(sourceplugin_patch PATCH_COMMAND /bin/sh -c "patch -p1 -i <SOURCE_DIR>/001-ffmpeg-all-libpostproc-plugin.patch && patch -p1 -i <SOURCE_DIR>/002-ffmpeg-libavutil-common-h-cpp11-constant-macros.patch")
else()
  set(sourceplugin_patch PATCH_COMMAND patch -p1 -i <SOURCE_DIR>/002-ffmpeg-libavutil-common-h-cpp11-constant-macros.patch)
endif()
list(APPEND sourceplugin_patch COMMAND patch -p1 -i <SOURCE_DIR>/008-ffmpeg-all-pgssubdec-use-caller-colorspace.patch)

if(CMAKE_C_FLAGS MATCHES "__PS4__" OR CMAKE_CXX_FLAGS MATCHES "__PS4__"
   OR CORE_SYSTEM_NAME STREQUAL "ps4" OR CORE_PLATFORM_NAME STREQUAL "ps4")
  # Kodi's patch series can restore FFmpeg's OpenSSL/GPL probe. Run after it.
  list(APPEND sourceplugin_patch COMMAND python3
       ${PROJECTSOURCE}/ps4/scripts/patch-ffmpeg-configure-openssl.py <SOURCE_DIR>/configure)
  list(APPEND sourceplugin_patch COMMAND python3
       ${PROJECTSOURCE}/ps4/scripts/patch-ffmpeg-ps4-cpu.py <SOURCE_DIR>/libavutil/cpu.c)
endif()

include(ExternalProject)
externalproject_add(ffmpeg
                    SOURCE_DIR ${CMAKE_SOURCE_DIR}
                    ${sourceplugin_patch}
                    CONFIGURE_COMMAND ${env_vars} <SOURCE_DIR>/configure
                      --prefix=${CMAKE_INSTALL_PREFIX}
                      --extra-version="kodi-${FFMPEG_VER}"
                      ${ffmpeg_conf}
                    BUILD_COMMAND ${MAKE_COMMAND})

install(CODE "Message(Done)")

# Quell warnings
set(BUILD_SHARED_LIBS)
set(XBMC_BUILD_DIR)
set(KODI_BUILD_DIR)
