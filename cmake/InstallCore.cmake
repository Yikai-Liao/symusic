include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(symusic_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/symusic")
set(symusic_vendor_includedir "${CMAKE_INSTALL_INCLUDEDIR}/symusic/vendor")
install(TARGETS symusic EXPORT symusicTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(TARGETS prestosynth samplerate
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    PUBLIC_HEADER DESTINATION "${symusic_vendor_includedir}"
)
install(DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    PATTERN "detail/zpp_impl.h" EXCLUDE
)
install(DIRECTORY
    3rdparty/fmt/include/
    3rdparty/minimidi/include/
    3rdparty/prestosynth/include/
    3rdparty/prestosynth/3rdparty/mio/include/
    3rdparty/prestosynth/3rdparty/eigen/Eigen
    DESTINATION "${symusic_vendor_includedir}"
)
install(FILES 3rdparty/pdqsort/pdqsort.h
    DESTINATION "${symusic_vendor_includedir}"
)
install(EXPORT symusicTargets NAMESPACE symusic:: DESTINATION "${symusic_package_dir}")
configure_package_config_file(
    cmake/symusicConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/symusicConfig.cmake"
    INSTALL_DESTINATION "${symusic_package_dir}"
    PATH_VARS CMAKE_INSTALL_LIBDIR
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/symusicConfig.cmake" DESTINATION "${symusic_package_dir}")
