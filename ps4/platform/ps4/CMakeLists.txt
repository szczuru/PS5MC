# PS4 platform sources (M2 – szkielet)
set(SOURCES
  main.cpp
  ${CMAKE_SOURCE_DIR}/ps4/src/PS4Privileges.cpp
)

set(HEADERS
  ${CMAKE_SOURCE_DIR}/ps4/src/PS4Privileges.h
)

core_add_library(platform_ps4)
