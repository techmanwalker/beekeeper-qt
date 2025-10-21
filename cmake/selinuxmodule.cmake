# cmake/selinux.cmake
#
# Build pipeline for SELinux module with optional m4 preprocessing and RPM scriptlets.
# Include AFTER your normal install() rules. Configure with -DBUILD_SELINUX_MODULE=ON
# to enable compilation and scriptlet generation.
#

option(BUILD_SELINUX_MODULE "Build and install SELinux policy module" OFF)

set(SELINUX_MODULE_NAME "beekeeper" CACHE STRING "Base name for SELinux module (policy_module name and output base)")
set(SELINUX_SRC_TE "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/burocracy/selinux.te")
set(SELINUX_SRC_FC_IN "${CMAKE_CURRENT_SOURCE_DIR}/src/polkit/burocracy/selinux.fc.in")
set(SELINUX_BUILD_DIR "${CMAKE_BINARY_DIR}/selinux")
file(MAKE_DIRECTORY "${SELINUX_BUILD_DIR}")

set(SELINUSD_TE_OUT   "${SELINUX_BUILD_DIR}/${SELINUX_MODULE_NAME}.te")
set(SELINUSD_TE_PROCESSED "${SELINUX_BUILD_DIR}/${SELINUX_MODULE_NAME}.te.m4")
set(SELINUSD_FC_OUT   "${SELINUX_BUILD_DIR}/${SELINUX_MODULE_NAME}.fc")
set(SELINUSD_MOD_OUT  "${SELINUX_BUILD_DIR}/${SELINUX_MODULE_NAME}.mod")
set(SELINUSD_PP_OUT   "${SELINUX_BUILD_DIR}/${SELINUX_MODULE_NAME}.pp")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_NORMALIZED_INSTALL_PREFIX "/usr")
else()
    set(CMAKE_NORMALIZED_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
endif()
set(CMAKE_NORMALIZED_INSTALL_FULL_DATADIR "${CMAKE_NORMALIZED_INSTALL_PREFIX}/share")

set(SELINUX_POLICY_INSTALL_DIR "${CMAKE_NORMALIZED_INSTALL_FULL_DATADIR}/selinux/packages" CACHE PATH "Install dir for compiled SELinux .pp packages")
set(SELINUX_TE_INSTALL_DIR    "${CMAKE_NORMALIZED_INSTALL_FULL_DATADIR}/selinux" CACHE PATH "Install dir for .te policy sources")
set(SELINUX_FCONTEXT_INSTALL_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/selinux/contexts/files" CACHE PATH "Install dir for fcontext files (.fc)")

if(NOT BUILD_SELINUX_MODULE)
    message(STATUS "SELinux: BUILD_SELINUX_MODULE=OFF — selinux support disabled; install rules still tolerate missing artifacts")
    install(FILES "${SELINUSD_TE_OUT}" DESTINATION "${SELINUX_TE_INSTALL_DIR}" RENAME "${SELINUX_MODULE_NAME}.te" OPTIONAL)
    install(FILES "${SELINUSD_FC_OUT}" DESTINATION "${SELINUX_FCONTEXT_INSTALL_DIR}" RENAME "${SELINUX_MODULE_NAME}.fc" OPTIONAL)
    install(FILES "${SELINUSD_PP_OUT}" DESTINATION "${SELINUX_POLICY_INSTALL_DIR}" RENAME "${SELINUX_MODULE_NAME}.pp" OPTIONAL)
    return()
endif()

message(STATUS "SELinux: BUILD_SELINUX_MODULE=ON — preparing selinux artifact build and RPM scriptlets")

find_program(M4_EXE NAMES m4)
find_program(CHECKMODULE_EXE NAMES checkmodule REQUIRED)
find_program(SEMODULE_PACKAGE_EXE NAMES semodule_package REQUIRED)

if(NOT CHECKMODULE_EXE)
    message(FATAL_ERROR "SELinux: 'checkmodule' not found. Install checkpolicy / selinux-policy-devel.")
endif()
if(NOT SEMODULE_PACKAGE_EXE)
    message(FATAL_ERROR "SELinux: 'semodule_package' not found. Install policycoreutils / selinux-policy-devel.")
endif()

if(NOT M4_EXE)
    message(WARNING "SELinux: 'm4' not found. TE will be used raw without preprocessing.")
endif()

# configure .fc (do @VAR@ substitutions)
if(EXISTS "${SELINUX_SRC_FC_IN}")
    configure_file("${SELINUX_SRC_FC_IN}" "${SELINUSD_FC_OUT}" @ONLY)
    message(STATUS "SELinux: configured ${SELINUSD_FC_OUT} from selinux.fc.in")
else()
    message(FATAL_ERROR "SELinux: missing ${SELINUX_SRC_FC_IN} (required when BUILD_SELINUX_MODULE=ON)")
endif()

# copy .te into build dir (raw); we prefer raw TE + optional m4 preprocessing
if(EXISTS "${SELINUX_SRC_TE}")
    configure_file("${SELINUX_SRC_TE}" "${SELINUSD_TE_OUT}" COPYONLY)
    message(STATUS "SELinux: copied ${SELINUSD_TE_OUT} (raw TE)")
else()
    message(FATAL_ERROR "SELinux: missing ${SELINUX_SRC_TE} (required when BUILD_SELINUX_MODULE=ON)")
endif()

# m4 include paths (semicolon-separated for CMake; user can override)
set(SELINUX_M4_INCLUDES "/usr/share/selinux/devel;/usr/share/selinux/devel/include" CACHE STRING "Semicolon-separated list of -I paths for m4 preprocessing.")

# If m4 exists, create processed TE
if(M4_EXE)
    # build includes string for shell
    set(SELINUX_M4_INCLUDES_RAW "")
    foreach(inc ${SELINUX_M4_INCLUDES})
        if(NOT "${inc}" STREQUAL "")
            string(APPEND SELINUX_M4_INCLUDES_RAW " -I ${inc}")
        endif()
    endforeach()

    add_custom_command(
        OUTPUT "${SELINUSD_TE_PROCESSED}"
        COMMAND /bin/sh -c "\"${M4_EXE}\" ${SELINUX_M4_INCLUDES_RAW} \"${SELINUSD_TE_OUT}\" > \"${SELINUSD_TE_PROCESSED}\""
        DEPENDS "${SELINUSD_TE_OUT}"
        COMMENT "SELinux: running m4 preprocessing -> ${SELINUSD_TE_PROCESSED}"
        VERBATIM
    )
    set(TE_USED_FOR_CHECKMODULE "${SELINUSD_TE_PROCESSED}")
else()
    set(TE_USED_FOR_CHECKMODULE "${SELINUSD_TE_OUT}")
endif()

# checkmodule: TE -> MOD (ensure depends on the TE used)
add_custom_command(
    OUTPUT "${SELINUSD_MOD_OUT}"
    COMMAND "${CHECKMODULE_EXE}" -M -m -o "${SELINUSD_MOD_OUT}" "${TE_USED_FOR_CHECKMODULE}"
    DEPENDS "${TE_USED_FOR_CHECKMODULE}" "${SELINUSD_FC_OUT}"
    COMMENT "SELinux: checkmodule -> ${SELINUSD_MOD_OUT}"
    VERBATIM
)

# semodule_package: MOD -> PP
add_custom_command(
    OUTPUT "${SELINUSD_PP_OUT}"
    COMMAND "${SEMODULE_PACKAGE_EXE}" -o "${SELINUSD_PP_OUT}" -m "${SELINUSD_MOD_OUT}"
    DEPENDS "${SELINUSD_MOD_OUT}"
    COMMENT "SELinux: semodule_package -> ${SELINUSD_PP_OUT}"
    VERBATIM
)

# Make module build part of default build so CPack sees the .pp available
add_custom_target(selinux-module ALL
    DEPENDS "${SELINUSD_PP_OUT}"
    COMMENT "SELinux: ${SELINUX_MODULE_NAME} module (compiled)"
)

# Install rules (optional)
install(FILES "${SELINUSD_TE_OUT}" DESTINATION "${SELINUX_TE_INSTALL_DIR}" RENAME "${SELINUX_MODULE_NAME}.te" OPTIONAL)
install(FILES "${SELINUSD_FC_OUT}" DESTINATION "${SELINUX_FCONTEXT_INSTALL_DIR}" RENAME "${SELINUX_MODULE_NAME}.fc" OPTIONAL)
install(FILES "${SELINUSD_PP_OUT}" DESTINATION "${SELINUX_POLICY_INSTALL_DIR}" RENAME "${SELINUX_MODULE_NAME}.pp" OPTIONAL)

# RPM scriptlets (omitted here for brevity; keep your existing scriptlet generation)
# ...
message(STATUS "SELinux: pipeline configured. Processed TE = ${TE_USED_FOR_CHECKMODULE}")
