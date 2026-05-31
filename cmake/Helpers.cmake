function(vkgs_status label value)
    if(VKGS_VERBOSE_CONFIGURE)
        message(STATUS "[vkgs] ${label}: ${value}")
    endif()
endfunction()

function(vkgs_declare_archive name url sha256)
    message(VERBOSE "[vkgs] FetchContent ${name}: ${url}")
    FetchContent_Declare(${name}
        URL "${url}"
        URL_HASH "SHA256=${sha256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
endfunction()
