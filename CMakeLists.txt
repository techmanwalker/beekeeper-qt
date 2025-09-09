cmake_minimum_required(VERSION 3.21)
project(beekeeper VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

# ------------------------------
# Qt6
# ------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus Concurrent LinguistTools)

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

set(BEEKEEPER_HELPER_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/beekeeper/bin/beekeeper-helper")

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
include(GNUInstallDirs)
set(BUILD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${BUILD_INCLUDE_DIR})

# Configure helperpath.hpp
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper/cmakedependentvariables/helperpath.hpp.in"
    "${BUILD_INCLUDE_DIR}/beekeeper/cmakedependentvariables/helperpath.hpp"
    @ONLY
)

# Configure systemd & DBus services
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

# Set up the helper path
add_compile_definitions(BEEKEEPER_HELPER_PATH="${BEEKEEPER_HELPER_PATH}")

# ------------------------------
# Core library
# ------------------------------
add_library(beekeeper SHARED
    src/core/beesdmgmt.cpp
    src/core/btrfsetup.cpp
    src/core/util.cpp
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
    src/cli/handlers.cpp
    src/core/util.cpp
)
set_target_properties(beekeeperman PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/lib/beekeeper"
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/beekeeper"
)
target_link_libraries(beekeeperman PRIVATE beekeeper)

# ------------------------------
# GUI sources
# ------------------------------
# --- Polkit sources (exclude beekeeper-helper.cpp)
file(GLOB POLKIT_SRCS src/polkit/*.cpp include/beekeeper/supercommander.hpp)
list(REMOVE_ITEM GUI_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/main.cpp)

# --- GUI sources (exclude main.cpp)
file(GLOB_RECURSE GUI_SRCS src/gui/*.cpp)
list(REMOVE_ITEM POLKIT_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/beekeeper-helper.cpp)

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
    BUILD_RPATH "${CMAKE_BINARY_DIR}/lib/beekeeper"
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/beekeeper"
)
target_link_libraries(beekeeper-qt PRIVATE
    Qt6::Core Qt6::Concurrent Qt6::Widgets Qt6::DBus
    beekeeper ${POLKIT_LIBS}
)

# ------------------------------
# Helper executable
# ------------------------------
add_executable(beekeeper-helper
    ${POLKIT_SRCS}
    src/polkit/beekeeper-helper.cpp
    src/cli/handlers.cpp
)
target_include_directories(beekeeper-helper PRIVATE
    ${BUILD_INCLUDE_DIR}
    ${POLKIT_INCLUDES}
)
target_link_libraries(beekeeper-helper PRIVATE
    Qt6::Core Qt6::Widgets Qt6::DBus Qt6::Concurrent
    beekeeper ${POLKIT_LIBS}
)

# For executables/libraries that include helperpath.hpp
target_compile_definitions(beekeeper-qt PRIVATE BEEKEEPER_HELPER_PATH="${BEEKEEPER_HELPER_PATH}")
target_compile_definitions(beekeeper-helper PRIVATE BEEKEEPER_HELPER_PATH="${BEEKEEPER_HELPER_PATH}")
target_compile_definitions(beekeeperman PRIVATE BEEKEEPER_HELPER_PATH="${BEEKEEPER_HELPER_PATH}")
target_compile_definitions(beekeeper PRIVATE BEEKEEPER_HELPER_PATH="${BEEKEEPER_HELPER_PATH}")

# Absolute RPATH
set_target_properties(beekeeper-helper PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/beekeeper"
    INSTALL_RPATH_USE_LINK_PATH TRUE
    BUILD_RPATH "${CMAKE_BINARY_DIR}/lib/beekeeper"
    SKIP_BUILD_RPATH FALSE
)

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
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/beekeeper
)

install(TARGETS beekeeperman
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS beekeeper-qt
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS beekeeper-helper
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/beekeeper/bin"
)

# systemd & DBus service dirs
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(SYSTEMD_DIR "/usr/lib/systemd/system")
    set(DBUS_DIR "/usr/share/dbus-1/system-services")
    set(POLKIT_ACTIONS_DIR "/usr/share/polkit-1/actions")
    set(DBUS_SYSTEM_DIR "/usr/share/dbus-1/system.d")
else()
    set(SYSTEMD_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/systemd/system")
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
    "${CMAKE_CURRENT_SOURCE_DIR}/beekeeper.pc.in"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# the desktop entry
install(FILES src/gui/entry.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/applications
        RENAME beekeeper-qt.desktop)

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
# Uninstall target
# ------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY
)
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

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
