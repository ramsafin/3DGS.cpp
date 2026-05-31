set(VKGS_HOST_TOOL_HINTS)
if(WIN32)
    file(GLOB VKGS_HOST_PYTHON_HINTS LIST_DIRECTORIES true
        "$ENV{LOCALAPPDATA}/Programs/Python/Python*"
    )
    file(GLOB VKGS_HOST_VULKAN_SDK_HINTS LIST_DIRECTORIES true
        "C:/VulkanSDK/*/Bin"
        "C:/VulkanSDK/*/bin"
    )
    list(APPEND VKGS_HOST_TOOL_HINTS ${VKGS_HOST_VULKAN_SDK_HINTS})
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
