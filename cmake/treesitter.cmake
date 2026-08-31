# Syntax highlighting comes from tree-sitter. A grammar carries the rules of
# its language and the queries/highlights.scm that names each piece of syntax,
# so slope holds no keyword table and no per-language code.
#
#   slope_language(NAME lua
#     REPO tree-sitter-grammars/tree-sitter-lua TAG v0.2.0
#     EXTENSIONS lua)
#
# QUERY_BASE_REPO/TAG prepends another grammar's highlights.scm, for a query
# written as the delta over another language.

# the release tarballs carry no top-level CMakeLists. lib/src/lib.c is the
# amalgamation upstream provides, so the runtime is one TU
CPMAddPackage(
  NAME tree-sitter-src
  GITHUB_REPOSITORY tree-sitter/tree-sitter
  GIT_TAG v0.25.10
  GIT_SHALLOW YES
  DOWNLOAD_ONLY YES
)

add_library(tree-sitter STATIC ${tree-sitter-src_SOURCE_DIR}/lib/src/lib.c)
target_include_directories(tree-sitter
  PRIVATE ${tree-sitter-src_SOURCE_DIR}/lib/src
  PUBLIC  ${tree-sitter-src_SOURCE_DIR}/lib/include)
if (NOT MSVC)
  target_compile_options(tree-sitter PRIVATE -w)
endif()

set(SLOPE_QUERY_BUILD_DIR ${CMAKE_BINARY_DIR}/slope_queries)
file(MAKE_DIRECTORY ${SLOPE_QUERY_BUILD_DIR})

set(SLOPE_GRAMMAR_TARGETS "" CACHE INTERNAL "")
set(SLOPE_GRAMMAR_EXTERNS "" CACHE INTERNAL "")
set(SLOPE_GRAMMAR_REGISTRATIONS "" CACHE INTERNAL "")

function(slope_language)
  cmake_parse_arguments(G "" "NAME;REPO;TAG;QUERY_BASE_REPO;QUERY_BASE_TAG"
                          "EXTENSIONS" ${ARGN})
  set(lib tree-sitter-${G_NAME})
  # shallow: a grammar's history is a generated parser.c rewritten over and
  # over, tens of times the size of the one revision that is wanted
  CPMAddPackage(NAME ${lib} GITHUB_REPOSITORY ${G_REPO} GIT_TAG ${G_TAG}
                GIT_SHALLOW YES DOWNLOAD_ONLY YES)

  # its own CMakeLists is ignored so the flags stay uniform
  set(src ${${lib}_SOURCE_DIR}/src/parser.c)
  if (EXISTS ${${lib}_SOURCE_DIR}/src/scanner.c)
    list(APPEND src ${${lib}_SOURCE_DIR}/src/scanner.c)
  endif()
  add_library(${lib} STATIC ${src})
  target_include_directories(${lib} PRIVATE ${${lib}_SOURCE_DIR}/src)
  if (NOT MSVC)
    target_compile_options(${lib} PRIVATE -w)   # generated code
  endif()

  set(parts "")
  if (G_QUERY_BASE_REPO)
    CPMAddPackage(NAME tree-sitter-${G_NAME}-base GITHUB_REPOSITORY ${G_QUERY_BASE_REPO}
                  GIT_TAG ${G_QUERY_BASE_TAG} GIT_SHALLOW YES DOWNLOAD_ONLY YES)
    list(APPEND parts ${tree-sitter-${G_NAME}-base_SOURCE_DIR}/queries/highlights.scm)
  endif()
  list(APPEND parts ${${lib}_SOURCE_DIR}/queries/highlights.scm)
  set(text "")
  foreach(part ${parts})
    file(READ ${part} chunk)
    string(APPEND text "${chunk}\n")
  endforeach()
  file(WRITE ${SLOPE_QUERY_BUILD_DIR}/${G_NAME}.scm "${text}")

  string(JOIN "\", \"" exts ${G_EXTENSIONS})
  set(targets ${SLOPE_GRAMMAR_TARGETS})
  list(APPEND targets ${lib})
  set(SLOPE_GRAMMAR_TARGETS "${targets}" CACHE INTERNAL "")
  set(SLOPE_GRAMMAR_EXTERNS
      "${SLOPE_GRAMMAR_EXTERNS}const TSLanguage* tree_sitter_${G_NAME}();\n"
      CACHE INTERNAL "")
  set(SLOPE_GRAMMAR_REGISTRATIONS
      "${SLOPE_GRAMMAR_REGISTRATIONS}    Code::RegisterGrammar(\"${G_NAME}\", [] { return (const void*)tree_sitter_${G_NAME}(); }, {\"${exts}\"});\n"
      CACHE INTERNAL "")
endfunction()

# Which of the built-in grammars this build wants. Each one costs a fetch, a
# few seconds of C, and its parser tables in every binary, so a project that
# only ever shows python drops the other two:
#
#   set(SLOPE_LANGUAGES python CACHE STRING "")
#
# An empty list builds no grammar at all, and Code draws every listing as
# plain text.
set(SLOPE_LANGUAGES "python;glsl;cpp;yaml" CACHE STRING
    "built-in grammars to build, any of: python glsl cpp yaml")

foreach(l ${SLOPE_LANGUAGES})
  if (NOT l MATCHES "^(python|glsl|cpp|yaml)$")
    message(FATAL_ERROR
      "SLOPE_LANGUAGES names '${l}', which slope does not ship. The built-in "
      "ones are python, glsl, cpp and yaml; anything else is added through "
      "SLOPE_EXTRA_LANGUAGES (see below).")
  endif()
endforeach()

if ("python" IN_LIST SLOPE_LANGUAGES)
  slope_language(NAME python
    REPO tree-sitter/tree-sitter-python TAG v0.23.6
    EXTENSIONS py)
endif()

if ("glsl" IN_LIST SLOPE_LANGUAGES)
  slope_language(NAME glsl
    REPO tree-sitter-grammars/tree-sitter-glsl TAG v0.2.0
    EXTENSIONS glsl frag vert comp geom tesc tese)
endif()

# the C++ query is only the delta over C, and C node types exist in C++
if ("cpp" IN_LIST SLOPE_LANGUAGES)
  slope_language(NAME cpp
    REPO tree-sitter/tree-sitter-cpp TAG v0.23.4
    EXTENSIONS cpp cxx cc h hpp hxx c
    QUERY_BASE_REPO tree-sitter/tree-sitter-c QUERY_BASE_TAG v0.23.4)
endif()

# its scanner includes the schema it was configured with, core by default
if ("yaml" IN_LIST SLOPE_LANGUAGES)
  slope_language(NAME yaml
    REPO tree-sitter-grammars/tree-sitter-yaml TAG v0.7.2
    EXTENSIONS yaml yml)
endif()

# A project using slope adds its own languages by setting this before it pulls
# slope in. One entry per language, "name|repo|tag|ext ext ...".
#
#   set(SLOPE_EXTRA_LANGUAGES
#     "rust|tree-sitter/tree-sitter-rust|v0.23.2|rs"
#     "lua|tree-sitter-grammars/tree-sitter-lua|v0.2.0|lua"
#     CACHE STRING "")
#   FetchContent_MakeAvailable(slope)
foreach(entry ${SLOPE_EXTRA_LANGUAGES})
  string(REPLACE "|" ";" fields "${entry}")
  list(LENGTH fields n)
  if (NOT n EQUAL 4)
    message(FATAL_ERROR "SLOPE_EXTRA_LANGUAGES entry \"${entry}\" is not name|repo|tag|extensions")
  endif()
  list(GET fields 0 l_name)
  list(GET fields 1 l_repo)
  list(GET fields 2 l_tag)
  list(GET fields 3 l_exts)
  string(REPLACE " " ";" l_exts "${l_exts}")
  slope_language(NAME ${l_name} REPO ${l_repo} TAG ${l_tag} EXTENSIONS ${l_exts})
endforeach()

configure_file(${PROJECT_SOURCE_DIR}/src/content/screen_primitives/text/Grammars.cpp.in
               ${CMAKE_BINARY_DIR}/slope_generated/Grammars.cpp @ONLY)
set(SLOPE_GENERATED_SOURCES ${CMAKE_BINARY_DIR}/slope_generated/Grammars.cpp)
