# xbmc/platform/ps4/CMakeLists.txt

set(SOURCES
  main.cpp
  PlatformPS4.cpp
  CPUInfoPS4.cpp
  MemUtils.cpp
  VideoOutInfo.cpp
  PS4InterfaceForCLog.cpp
  ${CMAKE_SOURCE_DIR}/ps4/src/PS4Privileges.cpp
)

set(HEADERS
  PlatformPS4.h
  CPUInfoPS4.h
  VideoOutInfo.h
  PS4InterfaceForCLog.h
  ${CMAKE_SOURCE_DIR}/ps4/src/PS4Privileges.h
)

core_add_library(platform_ps4)

add_subdirectory(audio)
add_subdirectory(input)
add_subdirectory(video)
# add_subdirectory(filesystem)  # later
