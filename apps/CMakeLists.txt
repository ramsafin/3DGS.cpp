if(VKGS_BUILD_VIEWER)
    add_subdirectory(viewer)
endif()

if(VKGS_BUILD_OFFSCREEN_APP)
    add_subdirectory(offscreen)
endif()

if(VKGS_BUILD_CAPS_APP)
    add_subdirectory(caps)
endif()
