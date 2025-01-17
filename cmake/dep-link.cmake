##+
## dep-link.cmake: Create the dependency interface libraries
##-
             
add_library(simh_regexp INTERFACE)
add_library(simh_video INTERFACE)

## Networking gets a bit screwy depending on whether USE_SHARED, USE_NETWORK
## and AIO are enabled.
##
## simh_nonetwork: No networking support. Used when the FEATURE_NETWORK option isn't
## part of an add_simulator()
add_library(simh_nonetwork STATIC)
target_sources(simh_nonetwork PRIVATE sim_ether.c)
target_link_libraries(simh_nonetwork PUBLIC os_features)

## simh_network: The non-AIO version of the network support
add_library(simh_network STATIC)
## simh_network_aio: The AIO version of the network support
add_library(simh_network_aio STATIC)
target_compile_definitions(simh_network_aio PUBLIC ${AIO_FLAGS})
        
## LIBPCAP is a special case
set(LIBPCAP_PROJECT "libpcap")
set(LIBPCAP_ARCHIVE_NAME "libpcap")
set(LIBPCAP_RELEASE "1.10.1")
set(LIBPCAP_ARCHIVE_TYPE "tar.gz")
set(LIBPCAP_TAR_ARCHIVE "${LIBPCAP_ARCHIVE_NAME}-${LIBPCAP_RELEASE}.${LIBPCAP_ARCHIVE_TYPE}")
set(LIBPCAP_SOURCE_URL  "https://github.com/the-tcpdump-group/libpcap/archive/refs/tags/${LIBPCAP_TAR_ARCHIVE}")

## And so is Npcap (the Windows version of libcap + device driver.)
set(NPCAP_PROJECT "npcap")
set(NPCAP_ARCHIVE_NAME "npcap-sdk")
set(NPCAP_RELEASE "1.13")
set(NPCAP_ARCHIVE_TYPE "zip")
set(NPCAP_SOURCE_URL "https://npcap.com/dist/${NPCAP_ARCHIVE_NAME}-${NPCAP_RELEASE}.${NPCAP_ARCHIVE_TYPE}")

## OpenVPN tap-windows gets downloaded as well...
set(TAPWINDOWS_PROJECT "tap-windows")
set(TAPWINDOWS_ARCHIVE_NAME "tap-windows")
set(TAPWINDOWS_RELEASE "master")
set(TAPWINDOWS_ARCHIVE_TYPE "zip")
set(TAPWINDOWS_TAR_ARCHIVE "${TAPWINDOWS_ARCHIVE_NAME}-${TAPWINDOWS_RELEASE}.${TAPWINDOWS_ARCHIVE_TYPE}")
set(TAPWINDOWS_SOURCE_URL "https://github.com/OpenVPN/tap-windows/archive/master.zip")
                                                             
function(fix_interface_libs _targ)
get_target_property(_aliased ${_targ} ALIASED_TARGET)
    if(NOT _aliased)
        set(fixed_libs)
        get_property(orig_libs TARGET ${_targ} PROPERTY INTERFACE_LINK_LIBRARIES)
        foreach(each_lib IN LISTS ${_lib})
            get_filename_component(stripped_lib "${each_lib}" DIRECTORY)
            if (stripped_lib)
                string(STRIP ${each_lib} stripped_lib)
                file(TO_CMAKE_PATH "${stripped_lib}" stripped_lib)
                if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
                  file(REAL_PATH "${stripped_lib}" stripped_lib)
                endif ()
            endif ()
            list(APPEND fixed_libs ${stripped_lib})
            message("** \"${each_lib}\" -> \"${stripped_lib}\"")
        endforeach ()
        set_property(TARGET ${_targ} PROPERTY INTERFACE_LINK_LIBRARIES ${fixed_libs})
    endif ()
endfunction ()

# Add sources to the simulators' network libraries
function (simh_network_sources)
    target_sources(simh_network PRIVATE ${ARGN})
    target_sources(simh_network_aio PRIVATE ${ARGN})
endfunction ()

function(simh_network_compile_definitions)
    cmake_parse_arguments(SIMH "" "" "PUBLIC;PRIVATE;INTERFACE" ${ARGN})
    if (SIMH_PUBLIC)
        target_compile_definitions(simh_network PUBLIC ${SIMH_PUBLIC})
        target_compile_definitions(simh_network_aio PUBLIC ${SIMH_PUBLIC})
    endif()
    if (SIMH_PRIVATE)
        target_compile_definitions(simh_network PRIVATE ${SIMH_PRIVATE})
        target_compile_definitions(simh_network_aio PRIVATE ${SIMH_PRIVATE})
    endif()
    if (SIMH_INTERFACE)
        target_compile_definitions(simh_network INTERFACE ${SIMH_INTERFACE})
        target_compile_definitions(simh_network_aio INTERFACE ${SIMH_INTERFACE})
    endif()
endfunction()

function(simh_network_include_directories)
    cmake_parse_arguments(SIMH "" "" "PUBLIC;PRIVATE;INTERFACE" ${ARGN})
    if (SIMH_PUBLIC)
        target_include_directories(simh_network PUBLIC ${SIMH_PUBLIC})
        target_include_directories(simh_network_aio PUBLIC ${SIMH_PUBLIC})
    endif()
    if (SIMH_PRIVATE)
        target_include_directories(simh_network PRIVATE ${SIMH_PRIVATE})
        target_include_directories(simh_network_aio PRIVATE ${SIMH_PRIVATE})
    endif()
    if (SIMH_INTERFACE)
        target_include_directories(simh_network INTERFACE ${SIMH_INTERFACE})
        target_include_directories(simh_network_aio INTERFACE ${SIMH_INTERFACE})
    endif()
endfunction()

function(simh_network_link_libraries)
    cmake_parse_arguments(SIMH "" "" "PUBLIC;PRIVATE;INTERFACE" ${ARGN})
    if (SIMH_PUBLIC)
        target_link_libraries(simh_network PUBLIC ${SIMH_PUBLIC})
        target_link_libraries(simh_network_aio PUBLIC ${SIMH_PUBLIC})
    endif()
    if (SIMH_PRIVATE)
        target_link_libraries(simh_network PRIVATE ${SIMH_PRIVATE})
        target_link_libraries(simh_network_aio PRIVATE ${SIMH_PRIVATE})
    endif()
    if (SIMH_INTERFACE)
        target_link_libraries(simh_network INTERFACE ${SIMH_INTERFACE})
        target_link_libraries(simh_network_aio INTERFACE ${SIMH_INTERFACE})
    endif()
endfunction()

## Ubuntu 16.04 -- when we find the SDL2 library, there are trailing spaces. Strip
## spaces from SDL2_LIBRARIES (and potentially others as we find them).
function (fix_libraries _lib)
    set(fixed_libs)
    foreach(each_lib IN LISTS ${_lib})
        get_filename_component(stripped_lib "${each_lib}" DIRECTORY)
        if (stripped_lib)
            string(STRIP ${stripped_lib} stripped_lib)
            file(TO_CMAKE_PATH "${stripped_lib}" stripped_lib)
            if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
              file(REAL_PATH "${stripped_lib}" stripped_lib)
            endif ()
        endif ()
        list(APPEND fixed_libs ${stripped_lib})
    endforeach ()
    set(${_lib} ${fixed_libs} PARENT_SCOPE)
endfunction ()

set(BUILD_WITH_VIDEO FALSE)
IF (WITH_VIDEO)
    ## +10 chaotic neutral hack: The SDL2_ttf CMake configuration include "-lfreetype" and
    ## "-lharfbuzz", but, if you're on MacOS, you need to tell the linker where these libraries
    ## are located...
    set(ldirs)
    foreach (lname ${FREETYPE_LIBRARIES} ${FREETYPE_LIBRARY} ${HARFBUZZ_LIBRARIES} ${HARFBUZZ_LIBRARY})
        get_filename_component(dirname "${lname}" DIRECTORY)
        if (dirname)
            string(STRIP ${dirname} dirname)
            file(TO_CMAKE_PATH "${dirname}" dirname)
            if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
              file(REAL_PATH "${dirname}" dirname)
            endif ()
            list(APPEND ldirs ${dirname})
        endif()
    endforeach ()
    get_property(ilink_dirs TARGET simh_video PROPERTY INTERFACE_LINK_DIRECTORIES)
    list(APPEND ilink_dirs ${ldirs})
    set_property(TARGET simh_video PROPERTY INTERFACE_LINK_DIRECTORIES ${ilink_dirs})
    unset(ilink_dirs)
    unset(ldirs)

    IF (SDL2_ttf_FOUND)
        IF (WIN32 AND TARGET SDL2_ttf::SDL2_ttf-static)
            target_link_libraries(simh_video INTERFACE SDL2_ttf::SDL2_ttf-static)
            list(APPEND VIDEO_PKG_STATUS "SDL2_ttf static")
        ELSEIF (TARGET SDL2_ttf::SDL2_ttf)
            target_link_libraries(simh_video INTERFACE SDL2_ttf::SDL2_ttf)
            list(APPEND VIDEO_PKG_STATUS "SDL2_ttf dynamic")
        ELSEIF (TARGET PkgConfig::SDL2_ttf)
            target_link_libraries(simh_video INTERFACE PkgConfig::SDL2_ttf)
            list(APPEND VIDEO_PKG_STATUS "pkg-config SDL2_ttf")
        ELSEIF (DEFINED SDL_ttf_LIBRARIES AND DEFINED SDL_ttf_INCLUDE_DIRS)
            target_link_libraries(simh_video INTERFACE ${SDL_ttf_LIBRARIES})
            target_include_directories(simh_video INTERFACE ${SDL_ttf_INCLUDE_DIRS})
            list(APPEND VIDEO_PKG_STATUS "detected SDL2_ttf")
        ELSE ()
            message(FATAL_ERROR "SDL2_ttf_FOUND set but no SDL2_ttf::SDL2_ttf import library or SDL_ttf_LIBRARIES/SDL_ttf_INCLUDE_DIRS? ")
        ENDIF ()
    ENDIF (SDL2_ttf_FOUND)

    IF (SDL2_FOUND)
        target_compile_definitions(simh_video INTERFACE USE_SIM_VIDEO HAVE_LIBSDL)
        ##
        ## Hopefully this hack can go away. Had to move the target_compile_definitions
        ## over to add_simulator.cmake to accomodate the BESM6 SDL irregularity.
        ##
        ## (keep)  if (CMAKE_HOST_APPLE)
        ## (keep)      ## NOTE: This shouldn't be just an Apple platform quirk; SDL_main should
        ## (keep)      ## be used by all platforms. <sigh!>
        ## (keep)      target_compile_definitions(simh_video INTERFACE SDL_MAIN_AVAILABLE)
        ## (keep)  endif ()

        ## Link to SDL2main if defined for this platform.
        target_link_libraries(simh_video INTERFACE $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>)

        IF (WIN32 AND TARGET SDL2::SDL2-static AND TARGET SDL2_ttf::SDL2_ttf-static)
            ## Prefer the static version on Windows, but only if SDL2_ttf is also static.
            target_link_libraries(simh_video INTERFACE SDL2::SDL2-static)
            list(APPEND VIDEO_PKG_STATUS "SDL2 static")
        ELSEIF (TARGET SDL2::SDL2)
            fix_interface_libs(SDL2::SDL2)
            target_link_libraries(simh_video INTERFACE SDL2::SDL2)
            list(APPEND VIDEO_PKG_STATUS "SDL2 dynamic")
        ELSEIF (TARGET PkgConfig::SDL2)
            fix_interface_libs(PkgConfig::SDL2)
            target_link_libraries(simh_video INTERFACE PkgConfig::SDL2)
            list(APPEND VIDEO_PKG_STATUS "pkg-config SDL2")
        ELSEIF (DEFINED SDL2_LIBRARIES AND DEFINED SDL2_INCLUDE_DIRS)
            fix_libraries(SDL2_LIBRARIES)
            target_link_libraries(simh_video INTERFACE ${SDL2_LIBRARIES})
            target_include_directories(simh_video INTERFACE ${SDL2_INCLUDE_DIRS})
            list(APPEND VIDEO_PKG_STATUS "detected SDL2")
        ELSE ()
            message(FATAL_ERROR "SDL2_FOUND set but no SDL2::SDL2 import library or SDL2_LIBRARIES/SDL2_INCLUDE_DIRS?")
        ENDIF ()
    ENDIF (SDL2_FOUND)

    IF (NOT USING_VCPKG AND FREETYPE_FOUND)
        if (TARGET Freetype::Freetype)
            target_link_libraries(simh_video INTERFACE freetype)
            list(APPEND VIDEO_PKG_STATUS "Freetype::Freetype")
        ELSEIF (TARGET PkgConfig::Freetype)
            target_link_libraries(simh_video INTERFACE PkgConfig::Freetype)
            list(APPEND VIDEO_PKG_STATUS "pkg-config Freetype")
        ELSE ()
            target_link_libraries(simh_video INTERFACE ${FREETYPE_LIBRARIES})
            target_include_directories(simh_video INTERFACE ${FREETYPE_INCLUDE_DIRS})
            list(APPEND VIDEO_PKG_STATUS "detected Freetype")
        ENDIF ()
    ENDIF ()

    IF (PNG_FOUND)
        target_compile_definitions(simh_video INTERFACE HAVE_LIBPNG)

        if (TARGET PNG::PNG)
            target_link_libraries(simh_video INTERFACE PNG::PNG)
            list(APPEND VIDEO_PKG_STATUS "interface PNG")
        elseif (TARGET PkgConfig::PNG)
            target_link_libraries(simh_video INTERFACE PkgConfig::PNG)
            list(APPEND VIDEO_PKG_STATUS "pkg-config PNG")
        else ()
            target_include_directories(simh_video INTERFACE ${PNG_INCLUDE_DIRS})
            target_link_libraries(simh_video INTERFACE ${PNG_LIBRARIES})
            list(APPEND VIDEO_PKG_STATUS "detected PNG")
        endif ()
    ENDIF (PNG_FOUND)

    set(BUILD_WITH_VIDEO TRUE)
ELSE ()
    set(VIDEO_PKG_STATUS "video support disabled")
ENDIF()

if (WITH_REGEX)
    ## TEMP: Use PCRE until patches for PCRE2 are avaiable.
    ##
    ## 1. Prefer PCRE2 over PCRE (unless PREFER_PCRE is set)
    ## 2. Prefer interface libraries before using detected find_package
    ##    variables.
    IF (TARGET PkgConfig::PCRE)
        target_link_libraries(simh_regexp INTERFACE PkgConfig::PCRE)
        if (PREFER_PCRE)
            target_compile_definitions(simh_regexp INTERFACE HAVE_PCRE_H)
            set(PCRE_PKG_STATUS "pkg-config pcre")
        else ()
            target_compile_definitions(simh_regexp INTERFACE HAVE_PCRE2_H)
            if (WIN32)
                ## Use static linkage (vice DLL) on Windows:
                target_compile_definitions(simh_regexp INTERFACE PCRE2_STATIC)
            endif ()
            set(PCRE_PKG_STATUS "pkg-config pcre2")
        endif ()
    ELSEIF (TARGET unofficial::pcre::pcre)
        ## vcpkg:
        target_link_libraries(simh_regexp INTERFACE unofficial::pcre::pcre)
        target_compile_definitions(simh_regexp INTERFACE HAVE_PCRE_H)
        target_compile_definitions(simh_regexp INTERFACE PCRE_STATIC)
        set(PCRE_PKG_STATUS "vcpkg pcre")
    ELSEIF (NOT PREFER_PCRE AND PCRE2_FOUND)
        target_compile_definitions(simh_regexp INTERFACE HAVE_PCRE2_H)
        target_include_directories(simh_regexp INTERFACE ${PCRE2_INCLUDE_DIRS})
        if (NOT WIN32)
            target_link_libraries(simh_regexp INTERFACE ${PCRE2_LIBRARY})
        else ()
            ## Use static linkage (vice DLL) on Windows:
            target_compile_definitions(simh_regexp INTERFACE PCRE2_STATIC)
        endif ()

        set(PCRE_PKG_STATUS "detected pcre2")
    ELSEIF (PCRE_FOUND)
        target_compile_definitions(simh_regexp INTERFACE HAVE_PCRE_H)
        target_include_directories(simh_regexp INTERFACE ${PCRE_INCLUDE_DIRS})
        target_link_libraries(simh_regexp INTERFACE ${PCRE_LIBRARY})
        if (WIN32)
            target_compile_definitions(simh_regexp INTERFACE PCRE_STATIC)
        endif ()
        set(PCRE_PKG_STATUS "detected pcre")
    endif ()
endif ()

if ((WITH_REGEX OR WITH_VIDEO) AND ZLIB_FOUND)
    target_compile_definitions(simh_regexp INTERFACE HAVE_ZLIB)
    target_compile_definitions(simh_video INTERFACE HAVE_ZLIB)
    if (TARGET ZLIB::ZLIB)
        target_link_libraries(simh_regexp INTERFACE ZLIB::ZLIB)
        target_link_libraries(simh_video INTERFACE ZLIB::ZLIB)
        set(ZLIB_PKG_STATUS "interface ZLIB")
    elseif (TARGET PkgConfig::ZLIB)
        target_link_libraries(simh_regexp INTERFACE PkgConfig::ZLIB)
        target_link_libraries(simh_video INTERFACE PkgConfig::ZLIB)
        set(ZLIB_PKG_STATUS "pkg-config ZLIB")
    else ()
        target_include_directories(simh_regexp INTERFACE ${ZLIB_INCLUDE_DIRS})
        target_link_libraries(simh_regexp INTERFACE ${ZLIB_LIBRARIES})
        target_include_directories(simh_video INTERFACE ${ZLIB_INCLUDE_DIRS})
        target_link_libraries(simh_video INTERFACE ${ZLIB_LIBRARIES})
        set(ZLIB_PKG_STATUS "detected ZLIB")
    endif ()
endif ()


if (WITH_NETWORK)
    ## Basic network support functions and UDP-based Ethernet
    simh_network_sources(sim_ether.c sim_networks/net_support.c sim_networks/udp_eth.c)
    if (WIN32)
        simh_network_sources(sim_networks/win32_utilities.c)
    endif ()
    simh_network_link_libraries(PUBLIC os_features thread_lib)

    ## Assume that SIMH will use the dynamically loaded pcap functions,
    ## the libpcap-devel's libraries are found.
    set(network_runtime USE_SHARED)

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/include"
    )

    set(pcap_platform)
    if (WITH_PCAP)
        find_package(PCAP)

        if (NOT PCAP_FOUND)
            if (NOT WIN32)
                set(pcap_platform "PCAP headers")

                message(STATUS "Downloading ${LIBPCAP_SOURCE_URL}")
                message(STATUS "Destination ${CMAKE_BINARY_DIR}/libpcap")
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/libpcap"
                    RESULT_VARIABLE LIBPCAP_MKDIR
                )
                if (NOT (${LIBPCAP_MKDIR} EQUAL 0))
                    message(FATAL_ERROR "Could not create ${CMAKE_CMAKE_BINARY_DIR}/libpcap")
                endif (NOT (${LIBPCAP_MKDIR} EQUAL 0))

                file(DOWNLOAD "${LIBPCAP_SOURCE_URL}" "${CMAKE_BINARY_DIR}/libpcap/libpcap.${LIBPCAP_ARCHIVE_TYPE}"
                        STATUS LIBPCAP_DOWNLOAD
                )
                list(GET LIBPCAP_DOWNLOAD 0 LIBPCAP_DL_STATUS)
                if (NOT (${LIBPCAP_DL_STATUS} EQUAL 0))
                    list(GET LIBPCAP_DOWNLOAD 1 LIBPCAP_DL_ERROR)
                    message(FATAL_ERROR "Download failed: ${LIBPCAP_DL_ERROR}")
                endif (NOT (${LIBPCAP_DL_STATUS} EQUAL 0))

                message(STATUS "Extracting headers ${LIBPCAP_SOURCE_URL}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E tar xvf "${CMAKE_BINARY_DIR}/libpcap/libpcap.${LIBPCAP_ARCHIVE_TYPE}"
                        "${LIBPCAP_PROJECT}-${LIBPCAP_ARCHIVE_NAME}-${LIBPCAP_RELEASE}/pcap.h"
                        "${LIBPCAP_PROJECT}-${LIBPCAP_ARCHIVE_NAME}-${LIBPCAP_RELEASE}/pcap/*.h"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/libpcap"
                    RESULT_VARIABLE LIBPCAP_EXTRACT
                )
                if (NOT (${LIBPCAP_EXTRACT} EQUAL 0))
                    message(FATAL_ERROR "Extract failed.")
                endif (NOT (${LIBPCAP_EXTRACT} EQUAL 0))

                message(STATUS "Copying headers from ${CMAKE_BINARY_DIR}/libpcap/${LIBPCAP_PROJECT}-${LIBPCAP_ARCHIVE_NAME}-${LIBPCAP_RELEASE}/pcap")
                message(STATUS "Destination ${CMAKE_BINARY_DIR}/include/pcap")
                execute_process(
                    COMMAND "${CMAKE_COMMAND}" -E copy_directory
                        "${LIBPCAP_PROJECT}-${LIBPCAP_ARCHIVE_NAME}-${LIBPCAP_RELEASE}/"
                        "${CMAKE_BINARY_DIR}/include/"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/libpcap"
                    RESULT_VARIABLE LIBPCAP_COPYDIR
                )
                if (NOT (${LIBPCAP_COPYDIR} EQUAL 0))
                    message(FATAL_ERROR "Copy failed.")
                endif (NOT (${LIBPCAP_COPYDIR} EQUAL 0))
            else ()
                ## Win32 Npcap path:
                set(pcap_platform "Win32/Win64 npcap")

                message(STATUS "Downloading ${NPCAP_SOURCE_URL}")
                message(STATUS "Destination ${CMAKE_BINARY_DIR}/npcap")
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/npcap"
                    RESULT_VARIABLE NPCAP_MKDIR
                )
                if (NOT (${NPCAP_MKDIR} EQUAL 0))
                    message(FATAL_ERROR "Could not create ${CMAKE_CMAKE_BINARY_DIR}/npcap")
                endif (NOT (${NPCAP_MKDIR} EQUAL 0))

                file(DOWNLOAD "${NPCAP_SOURCE_URL}" "${CMAKE_BINARY_DIR}/npcap/npcap.${NPCAP_ARCHIVE_TYPE}"
                        STATUS NPCAP_DOWNLOAD
                )
                list(GET NPCAP_DOWNLOAD 0 NPCAP_DL_STATUS)
                if (NOT (${NPCAP_DL_STATUS} EQUAL 0))
                    list(GET NPCAP_DOWNLOAD 1 NPCAP_DL_ERROR)
                    message(FATAL_ERROR "Download failed: ${NPCAP_DL_ERROR}")
                endif ()

                message(STATUS "Extracting headers ${NPCAP_SOURCE_URL}")
                execute_process(
                    # Can also include"Lib/"
                    COMMAND ${CMAKE_COMMAND} -E tar xvf "${CMAKE_BINARY_DIR}/npcap/npcap.${NPCAP_ARCHIVE_TYPE}"
                        "Include/"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/npcap"
                    RESULT_VARIABLE NPCAP_EXTRACT
                )
                if (NOT (${NPCAP_EXTRACT} EQUAL 0))
                    message(FATAL_ERROR "Extract failed.")
                endif ()

                message(STATUS "Copying headers from ${CMAKE_BINARY_DIR}/npcap/Include/pcap")
                message(STATUS "Destination ${CMAKE_BINARY_DIR}/include/pcap")
                execute_process(
                    COMMAND "${CMAKE_COMMAND}" -E copy_directory
                        "Include"
                        "${CMAKE_BINARY_DIR}/include/"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/npcap"
                    RESULT_VARIABLE NPCAP_COPYDIR
                )
                if (NOT (${NPCAP_COPYDIR} EQUAL 0))
                    message(FATAL_ERROR "Copy failed.")
                endif (NOT (${NPCAP_COPYDIR} EQUAL 0))
            endif ()

            ## And try finding it again...
            find_package(PCAP)
        elseif (NOT WIN32)
            set(pcap_platform "PCAP")
        elseif (WIN32)
            set(pcap_platform "Win32/Win64 npcap")
        endif ()

        if (PCAP_FOUND)
            foreach(hdr "${PCAP_INCLUDE_DIRS}")
              file(STRINGS ${hdr}/pcap/pcap.h hdrcontent REGEX "pcap_compile *\\(.*const")
              # message("hdrcontent: ${hdrcontent}")
              list(LENGTH hdrcontent have_bpf_const)
              if (${have_bpf_const} GREATER 0)
                message(STATUS "pcap_compile requires BPF_CONST_STRING")
                list(APPEND network_runtime BPF_CONST_STRING)
                break()
              endif()
            endforeach()

            ## If the PCAP_LIBRARIES are available, just use them. Don't bother with
            ## dynamic loading. (Ooops! Did we just unpack the npcap libraries? You
            ## have libpcap-devel installed? Huh. Maybe we use it or something. [sarcasm] :-)
            if (PCAP_LIBRARIES)
                set(network_runtime USE_NETWORK)
                simh_network_link_libraries(INTERFACE "${PCAP_LIBRARIES}")
            endif ()

            simh_network_sources(sim_networks/pcap_eth.c)
            simh_network_compile_definitions(PUBLIC HAVE_PCAP_NETWORK)
            simh_network_include_directories(PUBLIC "${PCAP_INCLUDE_DIRS}")

            if ("USE_SHARED" IN_LIST network_runtime)
                string(APPEND pcap_platform " dynamic load")
            else ()
                string(APPEND pcap_platform " platform libraries")
            endif ()

            list(APPEND NETWORK_PKG_STATUS ${pcap_platform})
        endif ()
    endif ()

    if (WIN32 AND OPENVPN_TUNTAP)
        find_path(TAPWINDOWS_HEADER
        NAMES
            tap-windows.h
        PATHS
            $(CMAKE_BINARY_DIR)/include
        )

        if (NOT TAPWINDOWS_HEADER)
            message(STATUS "Downloading ${TAPWINDOWS_SOURCE_URL}")
            message(STATUS "Destination ${CMAKE_BINARY_DIR}/tap-windows")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tap-windows"
                RESULT_VARIABLE TAPWINDOWS_MKDIR
            )
            if (NOT (${TAPWINDOWS_MKDIR} EQUAL 0))
                message(FATAL_ERROR "Could not create ${CMAKE_CMAKE_BINARY_DIR}/npcap")
            endif ()

            file(DOWNLOAD "${TAPWINDOWS_SOURCE_URL}" "${CMAKE_BINARY_DIR}/tap-windows/${TAPWINDOWS_TAR_ARCHIVE}"
                STATUS TAPWINDOWS_DOWNLOAD
            )
            list(GET TAPWINDOWS_DOWNLOAD 0 TAPWINDOWS_DL_STATUS)
            if (NOT (${TAPWINDOWS_DL_STATUS} EQUAL 0))
                list(GET TAPWINDOWS_DOWNLOAD 1 TAPWINDOWS_DL_ERROR)
                message(FATAL_ERROR "Download failed: ${TAPWINDOWS_DL_ERROR}")
            endif ()

            message(STATUS "Extracting headers ${TAPWINDOWS_SOURCE_URL}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xvf "${CMAKE_BINARY_DIR}/tap-windows/${TAPWINDOWS_TAR_ARCHIVE}"
                        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tap-windows"
                        RESULT_VARIABLE TAPWINDOWS_EXTRACT
            )
            if (NOT (${TAPWINDOWS_EXTRACT} EQUAL 0))
                message(FATAL_ERROR "Extract failed.")
            endif ()

            message(STATUS "Copying headers from ${CMAKE_BINARY_DIR}/tap-windows")
            message(STATUS "Destination ${CMAKE_BINARY_DIR}/include/")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy
                    "tap-windows-master/src/tap-windows.h"
                    "${CMAKE_BINARY_DIR}/include/"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tap-windows"
                RESULT_VARIABLE TAPWINDOWS_COPYDIR
            )
            if (NOT (${TAPWINDOWS_COPYDIR} EQUAL 0))
                message(FATAL_ERROR "Copy failed.")
            endif ()
        endif ()

        simh_network_compile_definitions(PUBLIC WITH_OPENVPN_TAPTUN)
        simh_network_include_directories(PUBLIC ${CMAKE_BINARY_DIR}/include)
        simh_network_sources(
            sim_networks/openvpn/argv.c
            sim_networks/openvpn/vpnintfs.c
            sim_networks/openvpn/opentap.c
            sim_networks/openvpn/apifuncs.c
        )
    endif ()

    if (WITH_VDE AND VDE_FOUND)
        if (TARGET PkgConfig::VDE)
            # simh_network_compile_definitions(PUBLIC $<TARGET_PROPERTY:PkgConfig::VDE,INTERFACE_COMPILE_DEFINITIONS>)
            # simh_network_include_directories(PUBLIC $<TARGET_PROPERTY:PkgConfig::VDE,INTERFACE_INCLUDE_DIRECTORIES>)
            simh_network_link_libraries(INTERFACE PkgConfig::VDE)
            list(APPEND NETWORK_PKG_STATUS "pkg-config VDE")
        else ()
            simh_network_include_directories(PUBLIC "${VDEPLUG_INCLUDE_DIRS}")
            simh_network_link_libraries(INTERFACE "${VDEPLUG_LIBRARY}")
            list(APPEND NETWORK_PKG_STATUS "detected VDE")
        endif ()

        simh_network_sources(sim_networks/vde_eth.c)
        simh_network_compile_definitions(PUBLIC HAVE_VDE_NETWORK)
    endif ()

    if (WITH_TAP)
        if (HAVE_TAP_NETWORK)
            simh_network_compile_definitions(PUBLIC ${NETWORK_TUN_DEFS} HAVE_TAP_NETWORK)
            simh_network_sources(sim_networks/tuntap_eth.c)

            if (HAVE_BSDTUNTAP)
                simh_network_compile_definitions(PUBLIC HAVE_BSDTUNTAP)
                list(APPEND NETWORK_PKG_STATUS "BSD TUN/TAP")
            else ()
                list(APPEND NETWORK_PKG_STATUS "TAP")
            endif ()
        endif (HAVE_TAP_NETWORK)
    endif (WITH_TAP)

    if (WITH_NETWORK AND WITH_SLIRP)
       simh_network_sources(sim_networks/sim_slirp/sim_slirp.c sim_networks/sim_slirp/slirp_poll.c)
        simh_network_compile_definitions(PUBLIC HAVE_SLIRP_NETWORK LIBSLIRP_STATIC)

        if (HAVE_INET_PTON)
            ## libslirp detects HAVE_INET_PTON for us.
            simh_network_compile_definitions(PUBLIC HAVE_INET_PTON)
        endif()

        simh_network_include_directories(
            PUBLIC
                ${CMAKE_SOURCE_DIR}/sim_networks/sim_slirp
                ${CMAKE_SOURCE_DIR}/libslirp/src
        )

        simh_network_link_libraries(PUBLIC slirp_static)
        list(APPEND NETWORK_PKG_STATUS "NAT(SLiRP)")
    endif ()

    ## Finally, set the network runtime if not already set.
    if (NOT ("USE_SHARED" IN_LIST network_runtime) AND NOT ("USE_NETWORK" IN_LIST network_runtime))
        ## Default to USE_SHARED... USE_NETWORK is deprecated.
        list(APPEND network_runtime "USE_SHARED")
    endif ()

    simh_network_compile_definitions(PUBLIC ${network_runtime})

    set(BUILD_WITH_NETWORK TRUE)
else (WITH_NETWORK)
    set(NETWORK_STATUS "networking disabled")
    set(NETWORK_PKG_STATUS "network disabled")
    set(BUILD_WITH_NETWORK FALSE)
endif (WITH_NETWORK)
