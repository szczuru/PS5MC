# cmake/platform/ps4/PS4.cmake
# Platform definition for PlayStation 4 (OpenOrbis + VideoOut)

set(PLATFORM_MAIN_SOURCE ${CMAKE_SOURCE_DIR}/xbmc/platform/ps4/main.cpp)

set(ARCH_DEFINES
  -DTARGET_POSIX
  -DTARGET_FREEBSD
  -DTARGET_PS4
  -D__PS4__
  -DHAS_GLES=2
  -DHAS_EGL=0          # Piglet eglGetDisplay fails for now – using VideoOut path
)

set(SYSTEM_DEFINES
  ${ARCH_DEFINES}
  -D_REENTRANT
  -D_FILE_OFFSET_BITS=64
  -D__STDC_CONSTANT_MACROS
)

set(PLATFORM_LINK_LIBS
  -lSceVideoOut
  -lSceAudioOut
  -lScePad
  -lSceUserService
  -lSceSystemService
  -lSceSysmodule
  -lkernel
  -lc
  -lm
)

# Optional: Piglet when it starts working
# list(APPEND PLATFORM_LINK_LIBS -lScePigletv2VSH)

set(APP_RENDER_SYSTEM gles CACHE STRING "" FORCE)
set(CORE_SYSTEM_NAME  ps4  CACHE STRING "" FORCE)
set(CORE_PLATFORM_NAME ps4 CACHE STRING "" FORCE)
