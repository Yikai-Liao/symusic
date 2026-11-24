cmake_minimum_required(VERSION 3.25)

set(vendor "${PROJECT_SOURCE_DIR}/3rdparty")
if(NOT EXISTS "${vendor}/fluidsynth/gcem/include/gcem.hpp")
    message(FATAL_ERROR "Initialize the git submodules recursively before building FluidSynth.")
endif()

set(BUILD_TESTING OFF)
set(ENABLE_TESTING OFF)
set(BUILD_SHARED_LIBS OFF)
set(INSTALL_DOCS OFF)
set(INSTALL_CMAKE_PACKAGE_MODULE OFF)

# libvorbis v1.3.7 predates the minimum policy version accepted by CMake 4.
# Scope the policy to this dependency.
block()
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0042 NEW)
    add_subdirectory("${vendor}/ogg" ogg EXCLUDE_FROM_ALL)
    # Non-alias targets remain visible outside the block's variable scope.
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/OggConfig.cmake" "")
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/OggConfigVersion.cmake"
        "set(PACKAGE_VERSION 1.3.6)\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n")
    add_subdirectory("${vendor}/vorbis" vorbis EXCLUDE_FROM_ALL)
endblock()
add_library(Vorbis::vorbisenc ALIAS vorbisenc)
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/VorbisConfig.cmake" "")

# libsndfile 1.2.2 enables Vorbis, FLAC and Opus as a group. Apply the
# Vorbis-only build changes to a private source copy.
set(sndfile_source "${CMAKE_CURRENT_BINARY_DIR}/libsndfile-source")
file(COPY "${vendor}/libsndfile/" DESTINATION "${sndfile_source}" PATTERN ".git" EXCLUDE)
# Restore patched inputs by content even when file(COPY) sees equal timestamps.
foreach(path IN ITEMS CMakeLists.txt cmake/SndFileChecks.cmake src/flac.c src/ogg_opus.c src/command.c)
    file(COPY_FILE "${vendor}/libsndfile/${path}" "${sndfile_source}/${path}" ONLY_IF_DIFFERENT)
endforeach()
include(VorbisOnly.cmake)

set(BUILD_SHARED_LIBS ON)
set(ENABLE_EXTERNAL_LIBS ON)
set(ENABLE_MPEG OFF)
set(ENABLE_EXPERIMENTAL OFF)
set(BUILD_PROGRAMS OFF)
set(BUILD_EXAMPLES OFF)
set(ENABLE_CPACK OFF)
set(CMAKE_DISABLE_FIND_PACKAGE_ALSA ON)
set(CMAKE_DISABLE_FIND_PACKAGE_Sndio ON)
block()
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0042 NEW)
    add_subdirectory("${sndfile_source}" sndfile EXCLUDE_FROM_ALL)
endblock()
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/SndFileConfig.cmake"
    "set(SndFile_VERSION 1.2.2)\nset(SndFile_WITH_EXTERNAL_LIBS TRUE)\n")
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/SndFileConfigVersion.cmake"
    "set(PACKAGE_VERSION 1.2.2)\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n")

foreach(feature IN ITEMS
        alsa aufile coreaudio coremidi dart dbus dsound framework ipv6 jack kai ladspa
        midishare native-dls network oboe openmp opensles oss pipewire portaudio pulseaudio
        readline sdl3 signalsmith systemd threads wasapi waveout winmidi)
    set(enable-${feature} OFF)
endforeach()
set(enable-libsndfile ON)
set(osal cpp11)
set(CMAKE_DISABLE_FIND_PACKAGE_OpenMP ON)
set(GCEM_INCLUDE_DIR "${vendor}/fluidsynth/gcem/include")
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
add_subdirectory("${vendor}/fluidsynth" upstream EXCLUDE_FROM_ALL)
add_custom_target(symusic_fluidsynth_runtime ALL DEPENDS libfluidsynth sndfile)

# Match each real filename to its ABI name so libsndfile can be found by that
# name at runtime and installation needs neither a symlink nor a second copy.
if(UNIX)
    foreach(library IN ITEMS libfluidsynth sndfile)
        get_target_property(abi_version ${library} SOVERSION)
        set_target_properties(${library} PROPERTIES VERSION "${abi_version}")
    endforeach()
endif()

if(APPLE)
    set_target_properties(sndfile PROPERTIES INSTALL_NAME_DIR "@rpath")
    set_target_properties(libfluidsynth PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set_target_properties(libfluidsynth PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
# Install just the runtime files. The vendored projects' install rules are
# excluded so that headers, import libraries and CLI tools stay out of the wheel.
set(destination symusic/_vendor/fluidsynth)
install(TARGETS libfluidsynth sndfile
    RUNTIME DESTINATION "${destination}"
    LIBRARY DESTINATION "${destination}" NAMELINK_SKIP
)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/_library.py"
    CONTENT [=[from pathlib import Path

LIBRARY_NAME = "$<TARGET_FILE_NAME:libfluidsynth>"
SNDFILE_NAME = "$<TARGET_FILE_NAME:sndfile>"


def library_path() -> Path:
    return Path(__file__).resolve().with_name(LIBRARY_NAME)
]=])
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/_library.py"
    DESTINATION "${destination}")
install(FILES "${vendor}/fluidsynth/LICENSE"
    DESTINATION "${destination}" RENAME FluidSynth.txt)
install(FILES "${vendor}/libsndfile/COPYING"
    DESTINATION "${destination}" RENAME libsndfile.txt)
install(FILES "${vendor}/ogg/COPYING"
    DESTINATION "${destination}" RENAME libogg.txt)
install(FILES "${vendor}/vorbis/COPYING"
    DESTINATION "${destination}" RENAME libvorbis.txt)
install(FILES "${vendor}/fluidsynth/gcem/LICENSE"
    DESTINATION "${destination}" RENAME GCEM.txt)
