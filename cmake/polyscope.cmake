if (TARGET polyscope)
  return()
endif()

# polyscope compiles with plenty of warnings of its own ; muting them here
# (rather than in our own flags) keeps warnings in slope's own code visible.
# -w only silenced this in Debug and only on GCC/Clang -- Release configs and
# MSVC (which needs /w, not -w) got the full noise regardless
set(CMAKE_CXX_FLAGS_OLD "${CMAKE_CXX_FLAGS}")
if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
endif()

# SYSTEM additionally marks polyscope's headers as external to whatever
# includes them (slope's own sources, tests/*), suppressing warnings raised
# by its inline/template code there too, not just in polyscope's own .cpp
# files (requires CMake >= 3.25 ; silently a no-op below that)
CPMAddPackage(
  NAME polyscope
  VERSION 2.6.1
  GITHUB_REPOSITORY "nmwsharp/polyscope"
  SYSTEM YES
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_OLD}")
