cmake_minimum_required(VERSION 3.10)
project(beekeeper)
set(PROJECT_VERSION 0.0.1)
set(PROJECT_VERSION_MAJOR 0)

# ------------ Qt6 ------------
find_package(Qt6 COMPONENTS Core Widgets DBus Concurrent REQUIRED)

# ------------ Polkit (optional) ------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(POLKIT QUIET polkit-gobject-1)
    if(POLKIT_FOUND)
        message(STATUS "Found Polkit: ${POLKIT_VERSION}")
    else()
        message(WARNING "Polkit not found - GUI will require manual privilege escalation")
    endif()
endif()

# ------------ Build type / debug flags ------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug logging enabled for Debug builds")
    set(ENABLE_BEEKEEPER_DEBUG 1)
elseif(DEFINED ENV{BEEKEEPER_DEBUG})
    message(STATUS "Debug logging enabled via environment variable")
    set(ENABLE_BEEKEEPER_DEBUG 1)
endif()

# ------------ C++ standard ------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------ RPATH ------------
set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib/beekeeper")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# ------------ Output dirs ------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ------------ Qt automoc ------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)


include(GNUInstallDirs)

# ====================================
# Core library
# ====================================
set_source_files_properties(
    src/core/beesdmgmt.cpp
    include/beekeeper/debug.hpp
    PROPERTIES
    COMPILE_FLAGS "-Wno-backslash-newline"
)

add_library(beesdmgmt SHARED
    src/core/beesdmgmt.cpp
    src/core/btrfsetup.cpp
    src/core/util.cpp
)

set_target_properties(beesdmgmt PROPERTIES
    OUTPUT_NAME beesdmgmt
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(beesdmgmt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ====================================
# CLI executable
# ====================================
add_executable(beekeeperman
    src/cli/beekeeperman.cpp
    src/cli/commandmachine/parser.cpp
)

target_link_libraries(beekeeperman beesdmgmt)

set_target_properties(beekeeperman PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib/beekeeper"
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

# ====================================
# GUI sources (Qt)
# ====================================
set(GUI_SRCS
    src/gui/main.cpp
    src/gui/mainwindow.cpp
    src/gui/rootshellthread.cpp
    src/gui/setupdialog.cpp
    src/gui/statusdotdelegate.cpp
    src/gui/uuidcolumndelegate.cpp
)

# Polkit/privileged GUI-side glue (NO helper main here!)
set(POLKIT_GUI_SRCS
    src/polkit/socketoperations.cpp
    src/polkit/superlaunch.cpp
    src/polkit/supercommander.cpp
    src/polkit/polkit.cpp
    include/beekeeper/supercommander.hpp
)

# Path to the privileged helper binary
set(BEEKEEPER_HELPER_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/beekeeper/bin/beekeeper-helper")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper/superlaunch.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/beekeeper/superlaunch.hpp"
    @ONLY
)

add_executable(beekeeper-qt
    ${GUI_SRCS}
    ${POLKIT_GUI_SRCS}
)

target_link_libraries(beekeeper-qt
    Qt6::Core
    Qt6::Concurrent
    Qt6::Widgets
    Qt6::DBus
    beesdmgmt
)

target_include_directories(beekeeper-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    /usr/include/polkit-qt6-1
)

set_target_properties(beekeeper-qt PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib/beekeeper"
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

add_executable(beekeeper-helper
    ${POLKIT_GUI_SRCS}
    src/polkit/beekeeper-helper.cpp
)


set_target_properties(beekeeper-helper PROPERTIES
    INSTALL_RPATH "$ORIGIN/../"
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

target_include_directories(beekeeper-helper PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# Link with supercommander + beesdmgmt
target_link_libraries(beekeeper-helper
    beesdmgmt
    Qt6::Core
    Qt6::Widgets
    Qt6::DBus
)

# ====================================
# Per-target compile defs
# ====================================
if(ENABLE_BEEKEEPER_DEBUG)
    target_compile_definitions(beesdmgmt PRIVATE BEEKEEPER_DEBUG_LOGGING)
    target_compile_definitions(beekeeperman PRIVATE BEEKEEPER_DEBUG_LOGGING)
    target_compile_definitions(beekeeper-qt PRIVATE BEEKEEPER_DEBUG_LOGGING)
endif()

# Pass it to the compiler only if the option is ON
if(BEEKEEPER_DEBUG_LOGGING)
    add_compile_definitions(BEEKEEPER_DEBUG_LOGGING=1)
endif()

# ====================================
# Install
# ====================================

install(TARGETS beesdmgmt
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/beekeeper
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/beekeeper
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/beekeeper
)

install(TARGETS beekeeperman
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS beekeeper-qt
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/beekeeper
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS beekeeper-helper
        RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/beekeeper/bin")

#install(FILES src/gui/beekeeper.desktop
#    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
#)

# ------------ Polkit action dir ------------
# By default, Polkit ignores /usr/local/share. If the prefix is still the default,
# override to /usr/share/polkit-1/actions. Otherwise follow DATADIR.


if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(POLKIT_ACTION_DIR "/usr/share/polkit-1/actions"
        CACHE PATH "Where to install polkit .policy files" FORCE)
else()
    set(POLKIT_ACTION_DIR "${CMAKE_INSTALL_FULL_DATADIR}/polkit-1/actions"
        CACHE PATH "Where to install polkit .policy files" FORCE)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/org.beekeeper.policy.in"
    "${CMAKE_CURRENT_BINARY_DIR}/org.beekeeper.policy"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.beekeeper.policy"
        DESTINATION "${POLKIT_ACTION_DIR}")

# --------------------------------------------------------------------
# Tests (never installed)
# Only build if -DBUILD_TESTS=ON
# --------------------------------------------------------------------

if(BUILD_TESTS)
    message(STATUS "Building Beekeeper tests...")

    # Non-Qt tests
    add_executable(beesdmgmttest tests/beesdmgmttest.cpp)
    target_link_libraries(beesdmgmttest PRIVATE beesdmgmt)

    add_executable(btrfsls tests/btrfsls.cpp)
    target_link_libraries(btrfsls PRIVATE beesdmgmt)

    add_executable(parserchecker tests/parserchecker.cpp src/cli/commandmachine/parser.cpp)
    target_link_libraries(parserchecker PRIVATE beesdmgmt)

    add_executable(setuppipeline tests/setuppipeline.cpp)
    target_link_libraries(setuppipeline PRIVATE beesdmgmt)

    # Qt-dependent test
    add_executable(supertest
        tests/supertest.cpp
        src/polkit/polkit.cpp
        src/polkit/socketoperations.cpp
        src/polkit/supercommander.cpp
        src/polkit/superlaunch.cpp
        include/beekeeper/supercommander.hpp
    )
    target_link_libraries(supertest PRIVATE beesdmgmt Qt6::Core Qt6::Widgets Qt6::DBus)
    set_target_properties(supertest PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
    if(ENABLE_BEEKEEPER_DEBUG)
        target_compile_definitions(supertest PRIVATE BEEKEEPER_DEBUG_LOGGING)
    endif()
endif()


# pkg-config
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/beekeeper.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/beekeeper.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/beekeeper.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)



# Uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()

# Summary
message(STATUS "")
message(STATUS "Beekeeper Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Qt6: ${Qt6_VERSION}")
message(STATUS "  Polkit: ${POLKIT_FOUND}")
if(POLKIT_FOUND)
    message(STATUS "    Version: ${POLKIT_VERSION}")
endif()
message(STATUS "")
