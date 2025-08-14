##-=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
## Git submodule setup.
##
## This script executes "git submodule init" for SIMH's submodule dependencies.
##-=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

## Standard submodules: (none at the moment)
set(SIMH_GIT_SUBMODS "")

find_package(Git)
if (GIT_FOUND)
    set(VCPKG_LAST_HASH "")

    ## On Windows (MS Visual C), we definitely want vcpkg and capture its status
    ## unless there is an existing vcpkg installation specified by the user. Don't
    ## try anything if the generator's toolset happens to be an XP toolset; vcpkg
    ## doesn't support XP.
    if (MSVC AND NOT CMAKE_GENERATOR_TOOLSET MATCHES "v[0-9][0-9][0-9]_xp" AND NOT DEFINED ENV{VCPKG_ROOT})
        list(APPEND SIMH_GIT_SUBMODS "vcpkg")

        ## Set the VCPKG_ROOT environment variable
        set(ENV{VCPKG_ROOT} ${CMAKE_SOURCE_DIR}/vcpkg)
        message(STATUS "Using vcpkg submodule as VCPKG_ROOT ($ENV{VCPKG_ROOT})")

        ## Grab the hash for HEAD for later comparison.
        execute_process(COMMAND ${GIT_EXECUTABLE} "submodule" "status" "vcpkg"
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                        OUTPUT_VARIABLE VCPKG_STATUS_OUTPUT
                        RESULT_VARIABLE VCPKG_STATUS_RESULT)
        if (NOT VCPKG_STATUS_RESULT)
            separate_arguments(VCPKG_STATUS_ARGS NATIVE_COMMAND ${VCPKG_STATUS_OUTPUT})
            if (VCPKG_STATUS_ARGS)
                ## The first argument is the hash, the second is the path.
                list(GET VCPKG_STATUS_ARGS 0 VCPKG_LAST_HASH)
            endif ()
        endif ()
    endif ()

    if (SIMH_GIT_SUBMODS)
        message(STATUS "Updating Git submodules: ${SIMH_GIT_SUBMODS}")

        execute_process(COMMAND "${GIT_EXECUTABLE}" "submodule" "update" "--init" "--recursive" ${SIMH_GIT_SUBMODS}
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                        RESULT_VARIABLE GIT_SUBMODULE_INIT_RESULT)

        if (NOT GIT_SUBMODULE_INIT_RESULT)
            set(do_vcpkg_boot TRUE)
            set(VCPKG_LAST_HASH2 "")
            if (VCPKG_LAST_HASH)
                execute_process(COMMAND ${GIT_EXECUTABLE} "submodule" "status" "vcpkg"
                                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                                OUTPUT_VARIABLE VCPKG_STATUS_OUTPUT2
                                RESULT_VARIABLE VCPKG_STATUS_RESULT2)

                if (NOT VCPKG_STATUS_RESULT2)
                    separate_arguments(VCPKG_STATUS_ARGS2 NATIVE_COMMAND ${VCPKG_STATUS_OUTPUT2})
                    list(GET VCPKG_STATUS_ARGS2 0 VCPKG_LAST_HASH2)
                    if (VCPKG_LAST_HASH STREQUAL VCPKG_LAST_HASH2)
                        set(do_vcpkg_boot FALSE)
                    endif ()
                endif ()
            endif ()

            if (do_vcpkg_boot)
                message(STATUS "Bootstrapping vcpkg (initial hash ${VCPKG_LAST_HASH}, current hash ${VCPKG_LAST_HASH2})")
                execute_process(COMMAND "bootstrap-vcpkg.bat"
                                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/vcpkg"
                                OUTPUT_VARIABLE VCPKG_BOOT_OUTPUT
                                ERROR_VARIABLE VCPKG_BOOT_ERROR
                                RESULT_VARIABLE VCPKG_BOOT_RESULT)
                if (VCPKG_BOOT_RESULT)
                    message(FATAL_ERROR "Failed to bootstrap vcpkg (status ${VCPKG_BOOT_RESULT})."
                        "\n"
                        "Output:\n${VCPKG_BOOT_OUTPUT}"
                        "\n"
                        "Error:\n${VCPKG_BOOT_ERROR}")
                endif ()
            endif ()
        else ()
            message(WARNING "Failed to initialize git submodules.")
        endif ()
    endif ()
else ()
    message(FATAL_ERROR "Git not found.")
endif ()