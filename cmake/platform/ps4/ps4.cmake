# PlayStation 4 uses OpenOrbis VideoOut and the native PS4 input/audio APIs.
set(APP_RENDER_SYSTEM gles CACHE STRING "" FORCE)
include_directories(${CMAKE_SOURCE_DIR}/ps4/src)
list(APPEND AUDIO_BACKENDS_LIST "ps4")
set(GL_INTERFACES_LIST "VideoOut")
