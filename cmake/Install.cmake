include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS 3dgs_core
    EXPORT 3dgsTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/3dgs DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT 3dgsTargets
    FILE 3dgsTargets.cmake
    NAMESPACE 3dgs::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/3dgs"
)

configure_package_config_file(
    cmake/3dgsConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/3dgs"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/3dgs"
)
