cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# some dependencies still ask for CMake < 3.5, which is now a hard error
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# pin every target to the same MSVC runtime, mixing /MT and /MD fails to link
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

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
include(luajit)

find_package(OpenGL REQUIRED)

## External tools configuration
find_package(LATEX COMPONENTS PDFLATEX REQUIRED)
# on Windows 'magick' must win, System32 ships an unrelated convert.exe
if (WIN32)
  find_program(SLOPE_CONVERT NAMES magick convert)
else()
  find_program(SLOPE_CONVERT NAMES convert magick)
endif()
if (NOT SLOPE_CONVERT)
  message(ERROR "Could not find the 'magick'/'convert' tool (imagemagick)")
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
configure_file(${PROJECT_SOURCE_DIR}/src/content/config/Options.cpp.in
               ${PROJECT_SOURCE_DIR}/src/content/config/Options.cpp)


file(GLOB SRC_lib
     "src/content/config/*"
     "src/content/authoring/*"
     "src/content/core/*"
     "src/content/screen_primitives/*"
     "src/content/screen_primitives/text/*"
     "src/content/screen_primitives/media/*"
     "src/content/screen_primitives/shapes/*"
     "src/content/screen_primitives/layout/*"
     "src/content/screen_primitives/gpu/*"
     "src/content/screen_primitives/widgets/*"
     "src/content/polyscope_primitives/*"
     "src/content/polyscope_primitives/quantities/*"
     "src/math/*"
     "src/slides/core/*"
     "src/slides/deck/*"
     "src/slides/deck/items/*"
     "src/slides/ui/*"
     "src/slides/capture/*"
     "src/slides/prompter/*"
     "src/slope.h"
     "src/extern/*"
)
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "src/*.h*")

## The library
add_library(slope STATIC ${SRC_lib})
target_link_libraries(slope PUBLIC polyscope Eigen3::Eigen spdlog OpenGL::GL fmt yaml-cpp luajit)
target_compile_features(slope PUBLIC cxx_std_20)
# MSVC's <cmath> only defines M_PI etc. when this is set, unlike glibc/libc++
target_compile_definitions(slope PUBLIC $<$<PLATFORM_ID:Windows>:_USE_MATH_DEFINES>)
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

option(BUILD_SLOPE_TESTS "Build the slope test suite" OFF)

if (BUILD_SLOPE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

