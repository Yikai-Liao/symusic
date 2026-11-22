symusic_add_dependency(Catch2)

add_executable(symusic_test test_main.cpp)
target_link_libraries(symusic_test PRIVATE Catch2::Catch2WithMain symusic)
set_property(TARGET symusic_test PROPERTY RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_custom_command(TARGET symusic_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/tests/testcases" "$<TARGET_FILE_DIR:symusic_test>/testcases"
)
add_test(NAME symusic_ctest COMMAND symusic_test)
set_tests_properties(symusic_ctest PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/tests")

if(NOT SYMUSIC_BUILD_PYTHON AND NOT CMAKE_CROSSCOMPILING)
    get_directory_property(consumer_link_options LINK_OPTIONS)
    configure_file(install_consumer/run.cmake.in run_install_consumer.cmake @ONLY)
    add_test(NAME symusic_install_consumer
        COMMAND "${CMAKE_COMMAND}" "-Dconfig=$<CONFIG>"
            -P "${CMAKE_CURRENT_BINARY_DIR}/run_install_consumer.cmake"
    )
endif()

if(NOT CMAKE_CROSSCOMPILING)
    add_library(fluid_runtime_fixture SHARED fluid_runtime_fixture.cpp)
    target_link_libraries(fluid_runtime_fixture PRIVATE Threads::Threads)
    set_target_properties(fluid_runtime_fixture PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_dependencies(symusic_test fluid_runtime_fixture)
    target_link_libraries(symusic_test PRIVATE Threads::Threads)
    target_compile_definitions(symusic_test PRIVATE
        SYMUSIC_TEST_FLUID_RUNTIME="$<TARGET_FILE:fluid_runtime_fixture>")
endif()
