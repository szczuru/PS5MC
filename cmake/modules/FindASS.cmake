#.rst:
# FindASS
# -------
# Finds the ASS library
#
# This will define the following target:
#
#   ${APP_NAME_LC}::ASS   - The ASS library
#

if(NOT TARGET ${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME})

  include(cmake/scripts/common/ModuleHelpers.cmake)

  macro(buildmacroASS)

    find_package(FreeType REQUIRED ${SEARCH_QUIET})
    find_package(HarfBuzz REQUIRED ${SEARCH_QUIET})
    find_package(FriBidi REQUIRED ${SEARCH_QUIET})
    find_package(Iconv REQUIRED ${SEARCH_QUIET})

    if(NOT (WIN32 OR WINDOWS_STORE))
      find_package(Fontconfig REQUIRED ${SEARCH_QUIET})
      # Static console Fontconfig needs its private Expat dependency at final link.
      find_package(EXPAT REQUIRED ${SEARCH_QUIET})
    endif()

    set(${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_VERSION ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_VER})

    if(WIN32 OR WINDOWS_STORE)
      set(patches "${CMAKE_SOURCE_DIR}/tools/depends/target/libass/01-win-CMakeLists.patch"
                  "${CMAKE_SOURCE_DIR}/tools/depends/target/libass/02-win-dwrite-fontload.patch"
                  "${CMAKE_SOURCE_DIR}/tools/depends/target/libass/03-win-dwrite-prevent-ass_set_fonts_dir.patch"
                  "${CMAKE_SOURCE_DIR}/tools/depends/target/libass/04-win-nullcheck-shared_hdc.patch")

      generate_patchcommand("${patches}")
      unset(patches)

      set(${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_DEBUG_POSTFIX d)
    endif()

    if(WIN32 OR WINDOWS_STORE)

      set(CMAKE_ARGS -DNATIVEPREFIX=${NATIVEPREFIX}
                     -DARCH=${ARCH}
                     ${LIBASS_ADDITIONAL_ARGS})
    else()
      find_program(AUTORECONF autoreconf REQUIRED)
      if (CMAKE_HOST_SYSTEM_NAME MATCHES "(Free|Net|Open)BSD")
        find_program(MAKE_EXECUTABLE gmake)
      endif()
      find_program(MAKE_EXECUTABLE make REQUIRED)

      if(ARCH STREQUAL x86_64)
        find_program(NASM nasm HINTS "${NATIVEPREFIX}/bin")
        if(NOT NASM_FOUND)
          set(DISABLE_ASM "--disable-asm")
        endif()
      elseif(NOT ARCH STREQUAL aarch64)
          set(DISABLE_ASM "--disable-asm")
      endif()

      if(PS4)
        set(ASS_HOST_ARG --host=x86_64-pc-freebsd)
      endif()

      set(CONFIGURE_COMMAND ${AUTORECONF} -vif
                    COMMAND ./configure
                              --prefix=${DEPENDS_PATH}
                              ${ASS_HOST_ARG}
                              --disable-shared
                              --disable-libunibreak
                              --enable-fontconfig
                              ${DISABLE_ASM})

      set(BUILD_COMMAND ${MAKE_EXECUTABLE})
      set(INSTALL_COMMAND ${MAKE_EXECUTABLE} install)
      set(BUILD_IN_SOURCE 1)

      # libass uses autotools rather than CMake, so it does not inherit the
      # compiler target and sysroot from the Kodi toolchain automatically.
      # Without this environment configure silently selects the host GCC and
      # produces a Linux archive that cannot be linked into the PS4 executable.
      if(PS4)
        set(DEP_BUILDENV "${CMAKE_COMMAND}" -E env
            "CC=${CMAKE_C_COMPILER} --target=${PS4_TRIPLE}"
            "CXX=${CMAKE_CXX_COMPILER} --target=${PS4_TRIPLE}"
            "AR=${CMAKE_AR}"
            "RANLIB=${CMAKE_RANLIB}"
            "CFLAGS=${CMAKE_C_FLAGS}"
            "CXXFLAGS=${CMAKE_CXX_FLAGS}"
            "LDFLAGS=${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_C_STANDARD_LIBRARIES}")
      endif()
    endif()

    BUILD_DEP_TARGET()
    unset(DEP_BUILDENV)
    unset(ASS_HOST_ARG)

    # Link libraries for target interface
    set(${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES ${APP_NAME_LC}::FriBidi ${APP_NAME_LC}::Iconv ${APP_NAME_LC}::HarfBuzz ${APP_NAME_LC}::FreeType)

    if(WIN32 OR WINDOWS_STORE)
      # Directwrite dependency
      list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES dwrite.lib)
    else()
      if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
        # Coretext dependencies
        list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES "-framework CoreText"
                                                                        "-framework CoreFoundation")
      endif()

      list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES Fontconfig::Fontconfig)
      list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES EXPAT::EXPAT)
      add_dependencies(${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME} Fontconfig::Fontconfig)
    endif()

    # Add dependencies to build target
    add_dependencies(${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME} ${APP_NAME_LC}::FriBidi
                                                                        ${APP_NAME_LC}::Iconv
                                                                        ${APP_NAME_LC}::HarfBuzz
                                                                        ${APP_NAME_LC}::FreeType)
  endmacro()

  set(${CMAKE_FIND_PACKAGE_NAME}_MODULE_LC libass)

  SETUP_BUILD_VARS()

  SETUP_FIND_SPECS()

  SEARCH_EXISTING_PACKAGES()

  if(("${${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME}_VERSION}" VERSION_LESS ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_VER} AND ENABLE_INTERNAL_ASS) OR
    (((CORE_SYSTEM_NAME STREQUAL linux AND NOT "webos" IN_LIST CORE_PLATFORM_NAME_LC) OR CORE_SYSTEM_NAME STREQUAL freebsd) AND ENABLE_INTERNAL_ASS))
    message(STATUS "Building ${${CMAKE_FIND_PACKAGE_NAME}_MODULE_LC}: \(version \"${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_VER}\"\)")
    cmake_language(EVAL CODE "
      buildmacro${CMAKE_FIND_PACKAGE_NAME}()
    ")
  endif()

  if(${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME}_FOUND)
    if(TARGET PkgConfig::${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME} AND NOT TARGET ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})
      add_library(${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME} ALIAS PkgConfig::${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME})
    elseif(TARGET libass::libass AND NOT TARGET ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})
      # Kodi custom libass target used for windows platforms
      add_library(${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME} ALIAS libass::libass)
    else()
      SETUP_BUILD_TARGET()
      add_dependencies(${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME} ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})
    endif()

    ADD_MULTICONFIG_BUILDMACRO()
  else()
    if(ASS_FIND_REQUIRED)
      message(FATAL_ERROR "Ass libraries were not found.")
    endif()
  endif()
endif()
