include_guard()

function(mtgs_copy_assets target_name src dst)
    message(STATUS "Defining custom asset copy target: ${target_name}")

    add_custom_target(${target_name}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dst}/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${src}"
            "${dst}/assets"
        DEPENDS "${src}"
        COMMENT "Syncing assets from ${src} to ${dst}/assets"
    )
endfunction()

function(mtgs_copy_dlls target_name src)
    if (EXISTS ${src} AND NOT ${src} STREQUAL "")
        if(WIN32)
            set(LIB_EXTENSION "*.dll")
        else()
            set(LIB_EXTENSION "*.so*") # Maps to .so, .so.1, etc.
        endif()

        file(GLOB_RECURSE DLL_FILES LIST_DIRECTORIES FALSE "${src}/${LIB_EXTENSION}")
        if(NOT DLL_FILES)
            message(WARNING "No libraries found matching ${LIB_EXTENSION} in ${src}. Skipping target creation.")
            return()
        endif()

        message(STATUS "Defining custom command for target ${target_name} copying directory ${src}")
        foreach(file_path IN LISTS DLL_FILES)
			message(STATUS "  - ${file_path}")
		endforeach()

        add_custom_target(${target_name}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DLL_FILES}
                "${CMAKE_BINARY_DIR}"

            DEPENDS ${src}
            COMMENT "Syncing DLL files for target ${target_name}"
            VERBATIM
        )
    else()
        message(FATAL_ERROR "Path not found: ${src}")
    endif()
endfunction()