cmake_minimum_required(VERSION 3.26.1)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version")
project(symusic_fluidsynth LANGUAGES C CXX)

set(vendor "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty")
if(NOT EXISTS "${vendor}/fluidsynth/gcem/include/gcem.hpp")
    message(FATAL_ERROR "Initialize the git submodules recursively before building FluidSynth.")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_TESTING OFF)
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
    set(enable-${feature} OFF CACHE BOOL "" FORCE)
endforeach()
set(enable-libsndfile ON CACHE BOOL "" FORCE)
set(osal cpp11 CACHE STRING "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_OpenMP ON)
set(GCEM_INCLUDE_DIR "${vendor}/fluidsynth/gcem/include")
add_subdirectory("${vendor}/fluidsynth" fluidsynth EXCLUDE_FROM_ALL)
add_custom_target(companion_runtime ALL DEPENDS libfluidsynth sndfile)

if(APPLE)
    set_target_properties(libfluidsynth PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set_target_properties(libfluidsynth PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
# Install just the runtime files. The vendored projects' install rules are
# excluded so that headers, import libraries and CLI tools stay out of the wheel.
install(TARGETS libfluidsynth sndfile
    RUNTIME DESTINATION symusic_fluidsynth
    LIBRARY DESTINATION symusic_fluidsynth NAMELINK_SKIP
)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/_library.py"
    CONTENT "LIBRARY_NAME = '$<TARGET_FILE_NAME:libfluidsynth>'\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/_library.py"
    DESTINATION symusic_fluidsynth)
install(FILES "${vendor}/fluidsynth/LICENSE"
    DESTINATION symusic_fluidsynth/licenses RENAME FluidSynth.txt)
install(FILES "${vendor}/libsndfile/COPYING"
    DESTINATION symusic_fluidsynth/licenses RENAME libsndfile.txt)
install(FILES "${vendor}/ogg/COPYING"
    DESTINATION symusic_fluidsynth/licenses RENAME libogg.txt)
install(FILES "${vendor}/vorbis/COPYING"
    DESTINATION symusic_fluidsynth/licenses RENAME libvorbis.txt)
install(FILES "${vendor}/fluidsynth/gcem/LICENSE"
    DESTINATION symusic_fluidsynth/licenses RENAME GCEM.txt)
install(FILES SOURCE.md DESTINATION symusic_fluidsynth/licenses)
