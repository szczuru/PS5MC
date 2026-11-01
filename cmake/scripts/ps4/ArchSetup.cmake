set(CORE_MAIN_SOURCE ${CMAKE_SOURCE_DIR}/xbmc/platform/ps4/main.cpp)
set(APP_BINARY_SUFFIX ".elf")
set(ARCH x86_64-freebsd)
set(ARCH_DEFINES -DTARGET_POSIX -DTARGET_FREEBSD -DTARGET_PS4 -D__PS4__)
set(SYSTEM_DEFINES -D__STDC_CONSTANT_MACROS -D_FILE_OFFSET_BITS=64
                   -D_REENTRANT -D_GNU_SOURCE)
set(PLATFORM_DIR platform/ps4)
set(PLATFORMDEFS_DIR platform/posix)
set(SYSTEM_LDFLAGS
    -lSceVideoOut -lSceAudioOut -lScePad -lSceUserService
    -lSceSystemService -lSceSysmodule)
set(AUDIO_BACKENDS_LIST "ps4")
set(GL_INTERFACES_LIST "VideoOut")
