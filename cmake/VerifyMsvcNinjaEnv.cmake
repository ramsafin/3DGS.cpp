if(NOT DEFINED ENV{INCLUDE} OR "$ENV{INCLUDE}" STREQUAL "")
    message(FATAL_ERROR
        "MSVC with Ninja requires a Visual Studio developer environment. "
        "Run this build from a Visual Studio Developer PowerShell/Command Prompt, or call VsDevCmd.bat first. "
        "The INCLUDE environment variable is empty."
    )
endif()

set(include_dirs "$ENV{INCLUDE}")
set(found_stdlib_header OFF)
foreach(include_dir IN LISTS include_dirs)
    if(EXISTS "${include_dir}/array" OR EXISTS "${include_dir}\\array")
        set(found_stdlib_header ON)
        break()
    endif()
endforeach()

if(NOT found_stdlib_header)
    message(FATAL_ERROR
        "MSVC standard library headers were not found in INCLUDE. "
        "Run this build from a Visual Studio Developer PowerShell/Command Prompt, or call VsDevCmd.bat first."
    )
endif()
