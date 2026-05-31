set(VKGS_HOST_TOOL_HINTS)
set(VKGS_HOST_VULKAN_SDK_ROOTS)
if(WIN32)
    if(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
        list(APPEND VKGS_HOST_VULKAN_SDK_ROOTS "$ENV{VULKAN_SDK}")
    endif()

    file(GLOB VKGS_DISCOVERED_VULKAN_SDK_ROOTS LIST_DIRECTORIES true
        "C:/VulkanSDK/*"
    )
    list(SORT VKGS_DISCOVERED_VULKAN_SDK_ROOTS COMPARE NATURAL ORDER DESCENDING)
    list(APPEND VKGS_HOST_VULKAN_SDK_ROOTS ${VKGS_DISCOVERED_VULKAN_SDK_ROOTS})
    list(REMOVE_DUPLICATES VKGS_HOST_VULKAN_SDK_ROOTS)

    file(GLOB VKGS_HOST_PYTHON_HINTS LIST_DIRECTORIES true
        "$ENV{LOCALAPPDATA}/Programs/Python/Python*"
    )

    foreach(sdk_root IN LISTS VKGS_HOST_VULKAN_SDK_ROOTS)
        if(EXISTS "${sdk_root}/Bin")
            list(APPEND VKGS_HOST_TOOL_HINTS "${sdk_root}/Bin")
        endif()
        if(EXISTS "${sdk_root}/bin")
            list(APPEND VKGS_HOST_TOOL_HINTS "${sdk_root}/bin")
        endif()

        if(CMAKE_SYSTEM_NAME STREQUAL "Windows"
            AND NOT Vulkan_INCLUDE_DIR
            AND EXISTS "${sdk_root}/Include"
            AND EXISTS "${sdk_root}/Lib/vulkan-1.lib"
        )
            set(Vulkan_INCLUDE_DIR "${sdk_root}/Include" CACHE PATH "Vulkan include directory" FORCE)
            set(Vulkan_LIBRARY "${sdk_root}/Lib/vulkan-1.lib" CACHE FILEPATH "Vulkan loader library" FORCE)
        endif()
    endforeach()
endif()

find_program(VKGS_HOST_PYTHON_EXECUTABLE
    NAMES python3 python python.exe
    HINTS
        ${VKGS_HOST_PYTHON_HINTS}
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT VKGS_HOST_PYTHON_EXECUTABLE)
    message(FATAL_ERROR
        "Could not find a host Python interpreter. "
        "Set VKGS_HOST_PYTHON_EXECUTABLE to a Python executable that runs on the build machine."
    )
endif()

find_program(VKGS_GLSLANG_VALIDATOR
    NAMES glslangValidator glslangValidator.exe
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
        ${VKGS_HOST_TOOL_HINTS}
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT VKGS_GLSLANG_VALIDATOR)
    message(FATAL_ERROR
        "Could not find host glslangValidator. "
        "Set VKGS_GLSLANG_VALIDATOR or install the Vulkan SDK on the build machine."
    )
endif()
