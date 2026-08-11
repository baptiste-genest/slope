cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(slope LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CPM)
include(eigen)
include(polyscope)
include(spdlog)
include(fmt)
include(yamlcpp)

find_package(OpenGL REQUIRED)

## External tools configuration
find_package(LATEX COMPONENTS PDFLATEX REQUIRED)
find_program(SLOPE_CONVERT convert)
if (NOT SLOPE_CONVERT)
  message(ERROR "Could not find the 'convert' tool (imagemagick)")
endif()
set(SLOPE_PDFLATEX ${PDFLATEX_COMPILER})

## Video playback is opt-in content, a missing ffmpeg is not a build error but
## a Video primitive that will refuse to play at runtime.
find_program(SLOPE_FFMPEG ffmpeg)
find_program(SLOPE_FFPROBE ffprobe)
if (NOT SLOPE_FFMPEG OR NOT SLOPE_FFPROBE)
  message(STATUS "ffmpeg/ffprobe not found, the Video primitive will be unavailable")
  set(SLOPE_FFMPEG ffmpeg)
  set(SLOPE_FFPROBE ffprobe)
endif()

## Where "#include <...>" inside a shader looks for the shader stdlib. Defaults
## to the source tree, which is what a build-from-source workflow wants; the
## install rule below also drops a copy under share/ for an installed slope.
set(SLOPE_SHADER_PATH "${PROJECT_SOURCE_DIR}/src/shaders/" CACHE PATH
    "Directory the shader stdlib (#include <...>) is read from")

message(STATUS "pdflatex compiler: ${SLOPE_PDFLATEX}")
message(STATUS "convert tool: ${SLOPE_CONVERT}")
message(STATUS "ffmpeg: ${SLOPE_FFMPEG}")
message(STATUS "shader stdlib: ${SLOPE_SHADER_PATH}")
configure_file(${PROJECT_SOURCE_DIR}/src/content/Options.cpp.in
               ${PROJECT_SOURCE_DIR}/src/content/Options.cpp)


file(GLOB SRC_lib
     "src/content/*"
     "src/content/screen_primitives/*"
     "src/content/polyscope_primitives/*"
     "src/content/polyscope_primitives/quantities/*"
     "src/content/Graphics2D/*"
     "src/content/screen_primitives/plots/*"
     "src/math/*"
     "src/slides/*"
     "src/style/*"
     "src/slope.h"
     "src/extern/*"
)
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "src/*.h*")

## The library
add_library(slope STATIC ${SRC_lib})
target_link_libraries(slope PUBLIC polyscope Eigen3::Eigen spdlog OpenGL::GL fmt yaml-cpp)
target_compile_features(slope PUBLIC cxx_std_20)
target_include_directories(slope PUBLIC src/ PRIVATE)
set_target_properties(slope PROPERTIES PUBLIC_HEADER "${HEADER_LIST}")

##INSTALL
install(TARGETS slope
        EXPORT slope
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        PUBLIC_HEADER DESTINATION include/slope
        )

install(DIRECTORY src/shaders/ DESTINATION share/slope/shaders
        FILES_MATCHING PATTERN "*.glsl")

option(BUILD_SLOPE_TUTORIALS "Build the slope tutorials" OFF)

if (BUILD_SLOPE_TUTORIALS)

## Tutorials
add_executable(tuto_helloworld tutorials/tuto_helloworld.cpp)
target_link_libraries(tuto_helloworld slope)

add_executable(tuto_mesh tutorials/tuto_mesh.cpp)
target_link_libraries(tuto_mesh slope)

endif()

