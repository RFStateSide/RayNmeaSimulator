# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
# Adapted from RF StateSide BrickSuite's CMake Deploy/build-tree convention.
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    get_filename_component(deploy_qt "${Qt6_DIR}/../../.." ABSOLUTE)
    set(RAYNMEA_QT_SOURCES "${deploy_qt}/../Src" CACHE PATH "Matching Qt source tree for complete runtime notices")
    set(RAYNMEA_ISCC "C:/Program Files (x86)/Inno Setup 6/ISCC.exe" CACHE FILEPATH "Inno Setup compiler")
    set(RAYNMEA_PACKAGE_VARIANT "native" CACHE STRING "Private package baseline: native or Debian12 (ARM64 Debian 12 environment only)")
    set_property(CACHE RAYNMEA_PACKAGE_VARIANT PROPERTY STRINGS native Debian12)
    add_custom_target(Deploy
        COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/deployment/deploy.py"
            --build "${CMAKE_BINARY_DIR}" --cmake "${CMAKE_COMMAND}"
        COMMENT "Build a curated Release runtime and private installer; see deployment/README.md"
        USES_TERMINAL VERBATIM)
endif()
