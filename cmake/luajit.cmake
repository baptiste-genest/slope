# LuaJIT ships no CMake build, so the source is fetched and its own build is
# driven once into the build tree (the CPM cache stays pristine, and the ~10s
# build never re-runs, nothing in slope can invalidate it).
CPMAddPackage(
  NAME luajit_src
  GITHUB_REPOSITORY LuaJIT/LuaJIT
  GIT_TAG v2.1
  DOWNLOAD_ONLY YES
)

set(LUAJIT_DIR ${CMAKE_BINARY_DIR}/luajit)
set(LUAJIT_INC ${LUAJIT_DIR}/src)

# the imported target advertises this at configure time, before the copy runs
file(MAKE_DIRECTORY ${LUAJIT_INC})

if (MSVC)
  # the POSIX makefile needs a gcc that cannot produce a lib MSVC links, and
  # bakes an unquoted PREFIX into the command line, which "Program Files" breaks
  set(LUAJIT_LIB ${LUAJIT_DIR}/src/lua51.lib)
  add_custom_command(
    OUTPUT ${LUAJIT_LIB}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${luajit_src_SOURCE_DIR} ${LUAJIT_DIR}
    COMMAND ${CMAKE_COMMAND} -E chdir ${LUAJIT_INC} cmd /c msvcbuild.bat static
    COMMENT "Building LuaJIT (once)"
    VERBATIM
  )
else()
  find_program(SLOPE_MAKE NAMES gmake make REQUIRED)
  set(LUAJIT_LIB ${LUAJIT_DIR}/src/libluajit.a)

  # LuaJIT refuses to build on macOS without this, and reads it from the
  # environment, where an empty CMAKE_OSX_DEPLOYMENT_TARGET leaves nothing
  if (APPLE)
    set(LUAJIT_MACOS_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if (NOT LUAJIT_MACOS_TARGET)
      execute_process(COMMAND sw_vers -productVersion
                      OUTPUT_VARIABLE LUAJIT_SW_VERS OUTPUT_STRIP_TRAILING_WHITESPACE)
      string(REGEX MATCH "^[0-9]+\\.[0-9]+" LUAJIT_MACOS_TARGET "${LUAJIT_SW_VERS}")
    endif()
    if (NOT LUAJIT_MACOS_TARGET)
      set(LUAJIT_MACOS_TARGET 11.0)
    endif()
    set(LUAJIT_ENV ${CMAKE_COMMAND} -E env MACOSX_DEPLOYMENT_TARGET=${LUAJIT_MACOS_TARGET})
  endif()

  add_custom_command(
    OUTPUT ${LUAJIT_LIB}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${luajit_src_SOURCE_DIR} ${LUAJIT_DIR}
    COMMAND ${LUAJIT_ENV} ${SLOPE_MAKE} -C ${LUAJIT_DIR} amalg BUILDMODE=static
    COMMENT "Building LuaJIT (once)"
    VERBATIM
  )
endif()

add_custom_target(luajit_build DEPENDS ${LUAJIT_LIB})

add_library(luajit STATIC IMPORTED GLOBAL)
set_target_properties(luajit PROPERTIES
  IMPORTED_LOCATION ${LUAJIT_LIB}
  INTERFACE_INCLUDE_DIRECTORIES ${LUAJIT_INC})
add_dependencies(luajit luajit_build)

# a 64-bit macOS executable linking LuaJIT needs its image below 2GB, or the
# JIT allocator cannot reach the generated code
if (APPLE)
  set(SLOPE_LUAJIT_EXE_LINK_FLAGS "-pagezero_size 10000 -image_base 100000000")
else()
  set(SLOPE_LUAJIT_EXE_LINK_FLAGS "")
endif()
