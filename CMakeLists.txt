cmake_minimum_required(VERSION 3.21)
project(beekeeper VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qt6-base (>= 6.5), qt6-tools (>= 6.5), libpolkit-qt6-1-1, libblkid, libudev")

set(CPACK_RPM_PACKAGE_REQUIRES
    "qt6-qtbase, qt6-tools, polkit-qt6-1, libblkid, libudev, systemd")

# ------------------------------
# Qt6
# ------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus Concurrent LinguistTools)

# ------------------------------
# SELinux Module -Part 1
# ------------------------------
option(BUILD_SELINUX_MODULE "Build and install SELinux policy module" OFF)

# ------------------------------
# Polkit
# ------------------------------
set(POLKIT_INCLUDES "")
set(POLKIT_LIBS "")

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(POLKIT_QT6 QUIET polkit-qt6-1)
    if(POLKIT_QT6_FOUND)
        message(STATUS "Found Polkit-Qt6: ${POLKIT_QT6_VERSION}")
        set(POLKIT_INCLUDES ${POLKIT_QT6_INCLUDE_DIRS})
        set(POLKIT_LIBS ${POLKIT_QT6_LIBRARIES})
    else()
        message(WARNING "Polkit-Qt6 not found - GUI will require manual privilege escalation")
    endif()
endif()

# ------------------------------
# NOTE: Compute dynamic install libdir for beekeeper to avoid hardcoding /lib or /lib64
# This gives:
#  - BEEKEEPER_INSTALL_LIBDIR -> the full absolute path (e.g. /usr/lib/beekeeper or /usr/lib64/beekeeper)
#  - BEEKEEPER_INSTALL_LIBDIR_REL -> the relative install destination (e.g. lib/beekeeper or lib64/beekeeper)
#  - BEEKEEPER_BUILD_LIBDIR -> build-time path inside the build tree for RPATHs (e.g. <builddir>/lib/beekeeper)
#
# Also: systemd units must go to the canonical system path (/usr/lib/systemd/system), not into lib64.
# ------------------------------

include(GNUInstallDirs)

# Full (absolute) install libdir for the packaged runtime libraries (e.g. /usr/lib/beekeeper or /usr/lib64/beekeeper)
set(BEEKEEPER_INSTALL_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}/beekeeper")
# Relative install destination for install(... DESTINATION ...) (e.g. lib/beekeeper or lib64/beekeeper)
set(BEEKEEPER_INSTALL_LIBDIR_REL "${CMAKE_INSTALL_LIBDIR}/beekeeper")
# Build-time RPATH directory inside the build tree (so binaries run from build/ can find libs)
set(BEEKEEPER_BUILD_LIBDIR "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/beekeeper")

# Path to the helper binary (used in configure_file replacements)
set(BEEKEEPER_HELPER_PATH "${BEEKEEPER_INSTALL_LIBDIR}/bin/thebeekeeper")

# Use canonical systemd directory for unit installation (avoid lib64 path)
if(NOT DEFINED SYSTEMD_DIR)
    set(SYSTEMD_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")
endif()

# ------------------------------
# Udev
# ------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUDEV REQUIRED libudev)

# ------------------------------
# Debug logging
# ------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(BEEKEEPER_DEBUG_LOGGING=1)
endif()

# ------------------------------
# Build directories & autogen
# ------------------------------
set(BUILD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${BUILD_INCLUDE_DIR})

# Configure systemd & DBus services (these .in files must reference @BEEKEEPER_INSTALL_LIBDIR@)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/burocracy/helper.service.in"
    "${CMAKE_CURRENT_BINARY_DIR}/helper.service" @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/burocracy/dbus.service.in"
    "${CMAKE_CURRENT_BINARY_DIR}/helper.dbus.service" @ONLY
)

# Configure policy file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/burocracy/policy.in"
    "${CMAKE_CURRENT_BINARY_DIR}/policy" @ONLY
)

# ----- SELinux Module - Part 2
# Include SELinux module build if requested
if(BUILD_SELINUX_MODULE)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/selinuxmodule.cmake)
endif()

# ------------------------------
# Core library
# ------------------------------
file(GLOB MGMT_SRCS
    src/core/management/*
)
file(GLOB UTIL_SRCS
    src/core/utils/*
)
add_library(beekeeper SHARED
    ${MGMT_SRCS}
    ${UTIL_SRCS}
)
set_target_properties(beekeeper PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)
target_include_directories(beekeeper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ------------------------------
# CLI executable
# ------------------------------
add_executable(beekeeperman
    src/cli/beekeeperman.cpp
    src/cli/commandmachine/parser.cpp
    src/cli/commandregistry.cpp
    src/cli/handlers.cpp
    ${UTIL_SRCS}
)
set_target_properties(beekeeperman PROPERTIES
    BUILD_RPATH "${BEEKEEPER_BUILD_LIBDIR}"
    INSTALL_RPATH "${BEEKEEPER_INSTALL_LIBDIR}"
)
target_link_libraries(beekeeperman PRIVATE beekeeper)

# ------------------------------
# GUI sources
# ------------------------------
# --- Polkit sources (exclude thebeekeeper.cpp)
file(GLOB POLKIT_SRCS
    include/beekeeper/supercommander.hpp
    src/polkit/auth.cpp
    src/polkit/globals.*           # matches globals.cpp / globals.hpp if needed
    src/polkit/multicommander.*   # matches multicommander.cpp / .hpp
    src/polkit/super*             # matches super*.cpp / .hpp
)

# --- GUI sources (exclude main.cpp)
file(GLOB_RECURSE GUI_SRCS src/gui/*.cpp)
list(REMOVE_ITEM POLKIT_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/diskwait.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/masterservice.cpp"
)

# ------------------------------
# GUI executable
# ------------------------------
add_executable(beekeeper-qt
    ${GUI_SRCS}
    ${POLKIT_SRCS}
    src/gui/main.cpp
)
target_include_directories(beekeeper-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${BUILD_INCLUDE_DIR}
    ${POLKIT_INCLUDES}
)
set_target_properties(beekeeper-qt PROPERTIES
    BUILD_RPATH "${BEEKEEPER_BUILD_LIBDIR}"
    INSTALL_RPATH "${BEEKEEPER_INSTALL_LIBDIR}"
)
target_link_libraries(beekeeper-qt PRIVATE
    Qt6::Core Qt6::Concurrent Qt6::Widgets Qt6::DBus
    beekeeper ${POLKIT_LIBS}
)

# ------------------------------
# Helper executable
# ------------------------------
add_executable(thebeekeeper
    ${POLKIT_SRCS}
    src/polkit/diskwait.cpp
    src/polkit/masterservice.cpp
    src/cli/commandregistry.cpp
    src/cli/commandmachine/parser.cpp
    src/cli/handlers.cpp
)

target_include_directories(thebeekeeper PRIVATE
    ${BUILD_INCLUDE_DIR}
    ${POLKIT_INCLUDES}
    ${LIBUDEV_INCLUDE_DIRS}
)

target_link_libraries(thebeekeeper PRIVATE
    Qt6::Core Qt6::Widgets Qt6::DBus Qt6::Concurrent
    beekeeper ${POLKIT_LIBS}
    ${LIBUDEV_LIBRARIES}
)

# Absolute RPATH for helper (use the computed full install libdir)
set_target_properties(thebeekeeper PROPERTIES
    BUILD_RPATH "${BEEKEEPER_BUILD_LIBDIR}"
    INSTALL_RPATH "${BEEKEEPER_INSTALL_LIBDIR}"
    INSTALL_RPATH_USE_LINK_PATH TRUE
    SKIP_BUILD_RPATH FALSE
)

# ------------------------------
# Optional libblkid support
# ------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBBLKID QUIET blkid)
    if(LIBBLKID_FOUND)
        message(STATUS "Found libblkid: ${LIBBLKID_VERSION}")

        # Define HAVE_LIBBLKID for all relevant targets
        add_compile_definitions(HAVE_LIBBLKID=1)

        # Include dirs & libraries for beekeeper (core library)
        target_include_directories(beekeeper PRIVATE ${LIBBLKID_INCLUDE_DIRS})
        target_link_libraries(beekeeper PRIVATE ${LIBBLKID_LIBRARIES})

        # Include dirs & libraries for CLI executable
        target_include_directories(beekeeperman PRIVATE ${LIBBLKID_INCLUDE_DIRS})
        target_link_libraries(beekeeperman PRIVATE ${LIBBLKID_LIBRARIES})

        # Include dirs & libraries for helper executable
        target_include_directories(thebeekeeper PRIVATE ${LIBBLKID_INCLUDE_DIRS})
        target_link_libraries(thebeekeeper PRIVATE ${LIBBLKID_LIBRARIES})
    else()
        message(WARNING "libblkid not found: falling back to blkid CLI parsing")
    endif()
endif()

# ------------------------------
# Translations
# ------------------------------
set(TRANSLATION_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/translations")
file(GLOB TRANSLATION_FILES "${TRANSLATION_SRC_DIR}/*.ts")

# Configure translationsdir.hpp
set(TRANSLATIONS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/beekeeper/translations")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper/cmakedependentvariables/translationsdir.hpp.in"
    "${BUILD_INCLUDE_DIR}/beekeeper/cmakedependentvariables/translationsdir.hpp"
    @ONLY
)

# Generate .qm files immediately without creating target dependencies
qt_add_translations(
    TARGETS beekeeper-qt            # Target that will use the .qm files
    TS_FILES ${TRANSLATION_FILES}   # Explicit .ts files
    QM_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/translations"
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)

# Install the .qm files
install(FILES ${QM_FILES} DESTINATION ${TRANSLATIONS_DIR})

# ------------------------------
# Enable debugging
# ------------------------------
if(ENABLE_BEEKEEPER_DEBUG)
    target_compile_definitions(beesdmgmt PRIVATE BEEKEEPER_DEBUG_LOGGING=1)
    target_compile_definitions(beekeeperman PRIVATE BEEKEEPER_DEBUG_LOGGING=1)
    target_compile_definitions(beekeeper-qt PRIVATE BEEKEEPER_DEBUG_LOGGING=1)
endif()

# Pass it to the compiler only if the option is ON 
if(BEEKEEPER_DEBUG_LOGGING)
    add_compile_definitions(BEEKEEPER_DEBUG_LOGGING=1)
endif()

# ------------------------------
# Installation
# ------------------------------
install(TARGETS beekeeper
    LIBRARY DESTINATION ${BEEKEEPER_INSTALL_LIBDIR_REL}
)

install(TARGETS beekeeperman
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS beekeeper-qt
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS thebeekeeper
    RUNTIME DESTINATION "${BEEKEEPER_INSTALL_LIBDIR_REL}/bin"
)

# systemd & DBus service dirs
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # Always prefer canonical system unit dir under /usr/lib/systemd/system for systemd units.
    set(SYSTEMD_DIR "/usr/lib/systemd/system")
    set(DBUS_DIR "/usr/share/dbus-1/system-services")
    set(POLKIT_ACTIONS_DIR "/usr/share/polkit-1/actions")
    set(DBUS_SYSTEM_DIR "/usr/share/dbus-1/system.d")
else()
    # When a custom prefix is used, install units under <prefix>/lib/systemd/system (still 'lib', not 'lib64').
    set(SYSTEMD_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")
    set(DBUS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/system-services")
    set(POLKIT_ACTIONS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/polkit-1/actions")
    set(DBUS_SYSTEM_DIR "${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/system.d")
endif()

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/helper.service"
    DESTINATION "${SYSTEMD_DIR}"
    RENAME org.beekeeper.helper.service
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/helper.dbus.service"
    DESTINATION "${DBUS_DIR}"
    RENAME org.beekeeper.Helper.service
)

# policy file
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/policy"
    DESTINATION "${POLKIT_ACTIONS_DIR}"
    RENAME org.beekeeper.policy
)

# dbus policy file
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/burocracy/dbus.conf"
    DESTINATION "${DBUS_SYSTEM_DIR}"
    RENAME org.beekeeper.dbus.conf
)

# pkg-config file
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/beekeeper.pc.in"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# the desktop entry
install(FILES src/gui/entry.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/applications
        RENAME beekeeper-qt.desktop)

set(INSTALL_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

add_custom_target(write-install-manifest ALL
    COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_MANIFEST=${INSTALL_MANIFEST}
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_write_install_manifest.cmake"
    COMMENT "Writing install manifest to ${INSTALL_MANIFEST}"
)

# ------------------------------
# Tests
# ------------------------------
if(BUILD_TESTS)
    file(GLOB TEST_SRCS tests/*.cpp)
    foreach(TEST_FILE ${TEST_SRCS})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        add_executable(
            ${TEST_NAME}
            ${TEST_FILE}
            ${POLKIT_SRCS}
            src/cli/commandmachine/parser.cpp
        )
        target_link_libraries(${TEST_NAME} PRIVATE beekeeper Qt6::Core Qt6::Widgets Qt6::DBus ${POLKIT_LIBS})
        target_include_directories(${TEST_NAME} PRIVATE
            ${BUILD_INCLUDE_DIR}
            ${POLKIT_INCLUDES}    # Polkit headers
        )
        # Apply debug compile definition if enabled
        if(ENABLE_BEEKEEPER_DEBUG)
            target_compile_definitions(${TEST_NAME} PRIVATE BEEKEEPER_DEBUG_LOGGING=1)
        endif()
    endforeach()
endif()

# ------------------------------
# Automatic documentation
# ------------------------------
if(MAKE_DOCS)
    find_package(Doxygen)

    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN  ${CMAKE_SOURCE_DIR}/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/Doxyfile)

        # Configure Doxyfile with CMake variables
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

        add_custom_target(doc
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    else()
        message(WARNING "Doxygen not found: documentation target will not be available")
    endif()
endif()

# ------------------------------
# Uninstall target
# ------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake" IMMEDIATE @ONLY
)
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake
)

# ------------------------------
# CPack configuration
# ------------------------------

# Metadata
set(CPACK_PACKAGE_NAME "beekeeper-qt")
set(CPACK_PACKAGE_VENDOR "Lito Parra")
set(CPACK_PACKAGE_DESCRIPTION "Deduplicate redundant data in your disk and save space")

# Generators: DEB, RPM
set(CPACK_GENERATOR "DEB;RPM")

# DEB package info
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lito Parra")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qt6-base (>= 6.5), qt6-tools (>= 6.5), libpolkit-qt6-1-1, libblkid1, libudev1")

# RPM package info
set(CPACK_RPM_PACKAGE_LICENSE "GPL3")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")

# Base requirements
set(CPACK_RPM_PACKAGE_REQUIRES
    "qt6-qtbase, qt6-qttools, polkit-qt6-1, libblkid, systemd-libs")

# ----- SELinux Module - Part 2 -----

# Add SELinux requirements if module is being built
if(BUILD_SELINUX_MODULE)
    set(CPACK_RPM_PACKAGE_REQUIRES "${BASE_RPM_REQUIRES}, policycoreutils, selinux-policy-base >= 3.13.1")
    
    # Create combined RPM scripts if SELinux module is enabled
    # Combine any existing scripts with SELinux scripts
    if(EXISTS "${CMAKE_BINARY_DIR}/rpm_selinux_post.sh")
        set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/rpm_selinux_post.sh")
    endif()
    if(EXISTS "${CMAKE_BINARY_DIR}/rpm_selinux_preun.sh")
        set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/rpm_selinux_preun.sh")
    endif()
    if(EXISTS "${CMAKE_BINARY_DIR}/rpm_selinux_postun.sh")
        set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/rpm_selinux_postun.sh")
    endif()
else()
    set(CPACK_RPM_PACKAGE_REQUIRES "${BASE_RPM_REQUIRES}")
endif()

# Optional: define package version (si quieres)
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_PACKAGE_CONTACT "Lito Parra <lito.15@proton.me>")

include(CPack)

# ------------------------------
# Summary
# ------------------------------
message(STATUS "")
message(STATUS "Beekeeper Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Qt6: ${Qt6_VERSION}")
message(STATUS "  Polkit: ${POLKIT_QT6_FOUND}")
if(POLKIT_QT6_FOUND)
    message(STATUS "    Version: ${POLKIT_QT6_VERSION}")
endif()
message(STATUS "")

