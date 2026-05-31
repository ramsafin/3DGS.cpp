function(vkgs_check_msvc_ninja_environment)
    if(NOT MSVC OR NOT CMAKE_GENERATOR MATCHES "Ninja")
        return()
    endif()

    if(NOT DEFINED ENV{INCLUDE} OR "$ENV{INCLUDE}" STREQUAL "")
        message(FATAL_ERROR
            "MSVC with the Ninja generator requires a Visual Studio developer environment. "
            "Run CMake from a Visual Studio Developer PowerShell/Command Prompt, or call "
            "VsDevCmd.bat before configuring/building. The INCLUDE environment variable is empty."
        )
    endif()

    include(CheckCXXSourceCompiles)
    unset(VKGS_MSVC_NINJA_STDLIB_HEADERS_FOUND CACHE)
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_cxx_source_compiles(
        "#include <array>
         #include <cstdint>
         int main() { std::array<std::uint32_t, 1> values{}; return static_cast<int>(values[0]); }"
         VKGS_MSVC_NINJA_STDLIB_HEADERS_FOUND
    )

    if(NOT VKGS_MSVC_NINJA_STDLIB_HEADERS_FOUND)
        message(FATAL_ERROR
            "MSVC standard library headers are not usable with the current Ninja build environment. "
            "Run CMake from a Visual Studio Developer PowerShell/Command Prompt, or call VsDevCmd.bat "
            "before configuring/building."
        )
    endif()
endfunction()

function(vkgs_add_msvc_ninja_environment_target)
    if(NOT MSVC OR NOT CMAKE_GENERATOR MATCHES "Ninja")
        return()
    endif()

    add_custom_target(vkgs_msvc_ninja_environment
        COMMAND ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/cmake/VerifyMsvcNinjaEnv.cmake"
        COMMENT "Checking MSVC Ninja build environment"
        VERBATIM
    )
endfunction()

function(vkgs_require_msvc_ninja_environment target)
    if(TARGET vkgs_msvc_ninja_environment AND TARGET ${target})
        add_dependencies(${target} vkgs_msvc_ninja_environment)
    endif()
endfunction()
