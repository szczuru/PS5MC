set(SOURCES DVDVideoCodecPS4.cpp
            VideoBufferPS4.cpp
            VideoDec2PS4.cpp)

set(HEADERS DVDVideoCodecPS4.h
            VideoBufferPS4.h
            VideoCodecRegistration.h
            VideoDec2PS4.h)

core_add_library(platform_ps4_video)
