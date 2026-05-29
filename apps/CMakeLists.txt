cmake_minimum_required(VERSION 3.26)

if (VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    add_subdirectory(viewer)
else ()
    add_subdirectory(offscreen)
endif ()
