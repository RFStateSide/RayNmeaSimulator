# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
# RayNmeaSimulator bundled Template catalog (parallel CMake authoring).
# Keep the marker even when the last JSON is deleted: rcc must still relink.
function(raynmea_template_files output isolated)
    set(directory "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    set(files "${directory}/catalog.txt")
    # Owner-approved v0.1.0 snapshots, identical in development and deployment.
    # Saving another authoring source does not approve it for compilation.
    foreach(example Default environment-weather navigation-autopilot single-engine synthetic-ais twin-engine)
        list(APPEND files "${directory}/${example}.json")
    endforeach()
    set(${output} ${files} PARENT_SCOPE)
endfunction()
