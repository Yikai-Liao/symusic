include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(symusic_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/symusic")
install(TARGETS symusic EXPORT symusicTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(TARGETS prestosynth samplerate
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(DIRECTORY
    include/
    3rdparty/fmt/include/
    3rdparty/minimidi/include/
    3rdparty/prestosynth/include/
    3rdparty/prestosynth/3rdparty/mio/include/
    3rdparty/prestosynth/3rdparty/eigen/Eigen
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(FILES 3rdparty/pdqsort/pdqsort.h 3rdparty/MetaMacro.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(EXPORT symusicTargets NAMESPACE symusic:: DESTINATION "${symusic_package_dir}")
configure_package_config_file(
    cmake/symusicConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/symusicConfig.cmake"
    INSTALL_DESTINATION "${symusic_package_dir}"
    PATH_VARS CMAKE_INSTALL_LIBDIR
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/symusicConfig.cmake" DESTINATION "${symusic_package_dir}")
