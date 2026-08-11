add_executable(test_latex test_latex.cpp)
target_link_libraries(test_latex PRIVATE slope)
add_test(NAME latex_compilation COMMAND test_latex)

add_executable(test_anchor test_anchor.cpp)
target_link_libraries(test_anchor PRIVATE slope)
add_test(NAME anchor_pos_persistence COMMAND test_anchor)

add_executable(test_paths test_paths.cpp)
target_link_libraries(test_paths PRIVATE slope)
add_test(NAME project_paths COMMAND test_paths)

add_executable(test_export test_export.cpp)
target_link_libraries(test_export PRIVATE slope)
target_compile_definitions(test_export PRIVATE SLOPE_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME export_pipeline COMMAND test_export)
# macOS CI has no way to get an OpenGL context at all (see test_export.cpp) ;
# it reports that with this exit code so ctest marks it skipped, not failed
set_tests_properties(export_pipeline PROPERTIES SKIP_RETURN_CODE 125)
