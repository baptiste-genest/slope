# LuaJIT ships no CMake build, so the source is fetched and its own Makefile is
# driven once into the build tree (the CPM cache stays pristine, and the ~10s
# build never re-runs : nothing in slope can invalidate it).
CPMAddPackage(
  NAME luajit_src
  GITHUB_REPOSITORY LuaJIT/LuaJIT
  GIT_TAG v2.1
  DOWNLOAD_ONLY YES
)

find_program(SLOPE_MAKE NAMES gmake make REQUIRED)

set(LUAJIT_DIR ${CMAKE_BINARY_DIR}/luajit)
set(LUAJIT_INC ${LUAJIT_DIR}/src)
set(LUAJIT_LIB ${LUAJIT_DIR}/src/libluajit.a)

# the imported target advertises this at configure time, before the copy runs
file(MAKE_DIRECTORY ${LUAJIT_INC})

add_custom_command(
  OUTPUT ${LUAJIT_LIB}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${luajit_src_SOURCE_DIR} ${LUAJIT_DIR}
  COMMAND ${SLOPE_MAKE} -C ${LUAJIT_DIR} amalg BUILDMODE=static
          MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
  COMMENT "Building LuaJIT (once)"
  VERBATIM
)
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
