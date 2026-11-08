# xbmc/platform/ps4/CMakeLists.txt

set(SOURCES
  PlatformPS4.cpp
  CPUInfoPS4.cpp
  ${CMAKE_SOURCE_DIR}/xbmc/platform/freebsd/GPUInfoFreebsd.cpp
  NetworkPS4.cpp
  MemUtils.cpp
  VideoOutInfo.cpp
  ${CMAKE_SOURCE_DIR}/ps4/src/PS4Privileges.cpp
  ${CMAKE_SOURCE_DIR}/ps4/src/PS4ThreadAtexit.c
)

set(HEADERS
  PlatformPS4.h
  PS4BootLog.h
  CPUInfoPS4.h
  VideoOutInfo.h
  ${CMAKE_SOURCE_DIR}/ps4/src/PS4Privileges.h
)

core_add_library(platform_ps4)
target_link_libraries(platform_ps4 PRIVATE
  ${CMAKE_SOURCE_DIR}/ps4/libXcape/lib/libXcape.a
)
