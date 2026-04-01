cmake_minimum_required(VERSION 3.20)

project(abcmidi_overlay C)
set(CMAKE_C_STANDARD 90)

set(ABCMIDI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../3rdparty/abcmidi")

set(abcmidi_abc2midi_sources
    "${CMAKE_CURRENT_LIST_DIR}/parseabc_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/store_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/genmidi_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/midifile_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/queues_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/parser2_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/stresspat_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/music_utils_wrapper.c"
)

set(abcmidi_midi2abc_sources
    "${CMAKE_CURRENT_LIST_DIR}/midifile_wrapper.c"
    "${CMAKE_CURRENT_LIST_DIR}/midi2abc_wrapper.c"
)

set(abcmidi_compile_definitions
    ANSILIBS
    HAVE_CONFIG_H
    _CRT_SECURE_NO_WARNINGS
)

function(abcmidi_add_executable target_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(
        ${target_name}
        PRIVATE
            "${ABCMIDI_SOURCE_DIR}"
            "${CMAKE_CURRENT_LIST_DIR}"
    )
    target_compile_definitions(${target_name} PRIVATE ${abcmidi_compile_definitions})

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wno-comment
                -Wno-deprecated-non-prototype
                -Wno-knr-promoted-parameter
                -Wno-parentheses
                -Wno-unused-value
        )
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wno-comment
                -Wno-old-style-definition
                -Wno-parentheses
                -Wno-strict-prototypes
                -Wno-unused-value
        )
    elseif(MSVC)
        target_compile_options(
            ${target_name}
            PRIVATE
                /wd4100
                /wd4996
        )
    endif()

    if(NOT MSVC)
        target_link_libraries(${target_name} PRIVATE m)
    endif()
endfunction()

abcmidi_add_executable(abc2midi ${abcmidi_abc2midi_sources})
abcmidi_add_executable(midi2abc ${abcmidi_midi2abc_sources})
