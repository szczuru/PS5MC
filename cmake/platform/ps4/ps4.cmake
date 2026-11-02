# PlayStation 4 uses OpenOrbis VideoOut and the native PS4 input/audio APIs.
set(APP_RENDER_SYSTEM gles CACHE STRING "" FORCE)
include_directories(${CMAKE_SOURCE_DIR}/ps4/src)
include_directories(${CMAKE_SOURCE_DIR}/ps4/libXcape/include)
list(APPEND AUDIO_BACKENDS_LIST "ps4")
set(GL_INTERFACES_LIST "VideoOut;egl")

# OpenOrbis exposes EGL and GLES through Piglet's VSH library rather than the
# desktop library names searched by Kodi's generic find modules.
list(APPEND PLATFORM_REQUIRED_DEPS OpenGLES EGL)
if(NOT TARGET kodi::OpenGLES)
  add_library(kodi::OpenGLES INTERFACE IMPORTED GLOBAL)
  set_target_properties(kodi::OpenGLES PROPERTIES
    INTERFACE_LINK_LIBRARIES "-lScePigletv2VSH"
    INTERFACE_INCLUDE_DIRECTORIES "${OO_PS4_TOOLCHAIN}/include"
    INTERFACE_COMPILE_DEFINITIONS "HAS_GLES=2;__PIGLET__=1")
endif()
if(NOT TARGET kodi::EGL)
  add_library(kodi::EGL INTERFACE IMPORTED GLOBAL)
  set_target_properties(kodi::EGL PROPERTIES
    INTERFACE_LINK_LIBRARIES "kodi::OpenGLES"
    INTERFACE_INCLUDE_DIRECTORIES "${OO_PS4_TOOLCHAIN}/include"
    INTERFACE_COMPILE_DEFINITIONS "HAS_EGL=1;__PIGLET__=1")
endif()
set(OpenGLES_FOUND TRUE)
set(EGL_FOUND TRUE)

list(APPEND PLATFORM_OPTIONAL_DEPS_EXCLUDE "^(Bluray|Udfread|XSLT|MariaDBClient|MySqlClient|MicroHttpd)$")
