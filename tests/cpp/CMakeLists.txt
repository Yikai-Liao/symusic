symusic_add_dependency(Catch2)

add_executable(symusic_test
    test_main.cpp
    test_abc_accompaniment.cpp
    test_abc_chords.cpp
    test_abc_diagnostics.cpp
    test_abc_directives.cpp
    test_abc_export.cpp
    test_abc_navigation.cpp
    test_abc_ornaments.cpp
    test_abc_pitch.cpp
    test_abc_rhythm.cpp
    test_abc_stress.cpp
    test_abc_syntax.cpp
    test_abc_underlay.cpp
    test_abc_written.cpp
)
target_link_libraries(symusic_test PRIVATE Catch2::Catch2WithMain symusic)
set_property(TARGET symusic_test PROPERTY RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_custom_command(TARGET symusic_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/tests/testcases" "$<TARGET_FILE_DIR:symusic_test>/testcases"
)
add_test(NAME symusic_ctest COMMAND symusic_test)
set_tests_properties(symusic_ctest PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/tests")
