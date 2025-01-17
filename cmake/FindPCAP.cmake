# Locate the PCAP library
#
# This module defines:
#
# ::
#
#   PCAP_LIBRARIES, the name of the library to link against
#   PCAP_INCLUDE_DIRS, where to find the headers
#   PCAP_FOUND, if false, do not try to link against
#   PCAP_VERSION_STRING - human-readable string containing the version of SDL_ttf
#
# Tweaks:
# 1. PCAP_PATH: A list of directories in which to search
# 2. PCAP_DIR: An environment variable to the directory where you've unpacked or installed PCAP.
#
# "scooter me fecit"

find_path(PCAP_INCLUDE_DIR 
    NAMES
        pcap.h
    HINTS
        ENV PCAP_DIR
    PATHS
        pcap
        PCAP
        ${PCAP_PATH}
    )

# if (CMAKE_SIZEOF_VOID_P EQUAL 8)
#     set(LIB_PATH_SUFFIXES lib64 x64 amd64 x86_64-linux-gnu aarch64-linux-gnu lib)
# else ()
#     set(LIB_PATH_SUFFIXES x86)
# endif ()

find_library(PCAP_LIBRARY
        NAMES
            pcap pcap_static libpcap libpcap_static wpcap
        HINTS
            ENV PCAP_DIR
        PATH_SUFFIXES
            ${LIB_PATH_SUFFIXES}
        PATHS
            ${PCAP_PATH}
            ${CMAKE_BINARY_DIR}/npcap/Lib
        )
# ## message(STATUS "LIB_PATH_SUFFIXES ${LIB_PATH_SUFFIXES}")
## message(STATUS "pcap library is ${PCAP_LIBRARY}")

if (WIN32 AND PCAP_LIBRARY)
    ## Only worry about the packet library on Windows.
    find_library(PACKET_LIBRARY
        NAMES
            packet Packet
        HINTS
            ENV PCAP_DIR
        PATH_SUFFIXES
            ${LIB_PATH_SUFFIXES}
        PATHS
            ${PCAP_PATH}
            ${CMAKE_BINARY_DIR}/npcap/Lib
    )
else ()
    set(PACKET_LIBRARY)
endif (WIN32 AND PCAP_LIBRARY)
## message(STATUS "packet library is ${PACKET_LIBRARY}")

set(PCAP_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})

set(PCAP_LIBRARIES)
if (PCAP_LIBRARY)
    list(APPEND PCAP_LIBRARIES ${PCAP_LIBRARY})
    if (PACKET_LIBRARY)
        list(APPEND PCAP_LIBRARIES ${PACKET_LIBRARY})
    endif ()
endif ()

unset(PCAP_LIBRARY)
unset(PCAP_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
if (PCAP_LIBRARIES)
    find_package_handle_standard_args(
        PCAP
        REQUIRED_VARS
            PCAP_LIBRARIES
            PCAP_INCLUDE_DIRS
    )
else ()
    ## Dynamic, headers only
    find_package_handle_standard_args(
        PCAP
        REQUIRED_VARS
            PCAP_INCLUDE_DIRS
    )
endif ()
