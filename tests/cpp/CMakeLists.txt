symusic_add_dependency(Catch2)

add_executable(symusic_test test_main.cpp test_abc.cpp)
target_link_libraries(symusic_test PRIVATE Catch2::Catch2WithMain symusic)
set_property(TARGET symusic_test PROPERTY RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_custom_command(TARGET symusic_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/tests/testcases" "$<TARGET_FILE_DIR:symusic_test>/testcases"
)
add_test(NAME symusic_ctest COMMAND symusic_test)
set_tests_properties(symusic_ctest PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/tests")
