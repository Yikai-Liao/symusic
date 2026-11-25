# A macro preserves dependencies' parent-scope package exports.
# Only dependency status chatter is hidden; explicit user log levels win.
macro(symusic_add_dependency name)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/3rdparty/${name}/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing ${name} submodule. Run git submodule update --init --recursive --depth 1."
        )
    endif()
    set(_symusic_restore_log_level FALSE)
    if(NOT DEFINED CMAKE_MESSAGE_LOG_LEVEL)
        set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
        set(_symusic_restore_log_level TRUE)
    endif()
    add_subdirectory(
        "${PROJECT_SOURCE_DIR}/3rdparty/${name}"
        "${PROJECT_BINARY_DIR}/3rdparty/${name}"
        EXCLUDE_FROM_ALL
    )
    if(_symusic_restore_log_level)
        unset(CMAKE_MESSAGE_LOG_LEVEL)
    endif()
endmacro()

symusic_add_dependency(fmt)
symusic_add_dependency(minimidi)
