# Installs the library, its headers and a CMake package, so that consumers can use
# find_package(basalt) against an install prefix or an unpacked release archive.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(BASALT_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/basalt")

install(TARGETS basalt EXPORT basaltTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ generated/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT basaltTargets NAMESPACE basalt:: DESTINATION ${BASALT_CMAKE_DIR})

configure_package_config_file(cmake/basaltConfig.cmake.in
  "${PROJECT_BINARY_DIR}/basaltConfig.cmake"
  INSTALL_DESTINATION ${BASALT_CMAKE_DIR})
# Before 1.0, a minor version may break the API.
write_basic_package_version_file("${PROJECT_BINARY_DIR}/basaltConfigVersion.cmake"
  COMPATIBILITY SameMinorVersion)
install(FILES
  "${PROJECT_BINARY_DIR}/basaltConfig.cmake"
  "${PROJECT_BINARY_DIR}/basaltConfigVersion.cmake"
  DESTINATION ${BASALT_CMAKE_DIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/basalt OPTIONAL)

set(CPACK_PACKAGE_NAME basalt)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Basalt")
if(WIN32)
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
