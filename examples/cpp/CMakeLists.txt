symusic_add_dependency(nanobench)

foreach(example IN ITEMS note_count dump io_bench synth adjust_time process_midi_directory)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE symusic)
    set_property(TARGET ${example} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
endforeach()
target_link_libraries(io_bench PRIVATE nanobench)
