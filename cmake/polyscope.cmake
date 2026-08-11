if (TARGET polyscope)
  return()
endif()

# mute polyscope's own warnings so ours stay visible
set(CMAKE_CXX_FLAGS_OLD "${CMAKE_CXX_FLAGS}")
if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
endif()

# SYSTEM also marks its headers external where we include them, needs CMake 3.25
CPMAddPackage(
  NAME polyscope
  VERSION 2.6.1
  GITHUB_REPOSITORY "nmwsharp/polyscope"
  SYSTEM YES
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_OLD}")
