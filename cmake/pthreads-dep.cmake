#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
# Manage the pthreads dependency
#
# (a) Try to locate the system's installed pthreads library, which is very
#     platform dependent (MSVC -> Pthreads4w, MinGW -> pthreads, *nix -> pthreads.)
# (b) MSVC: Build Pthreads4w as a dependent
#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

if (TARGET thread_lib)
    return()
endif()

include(CheckIncludeFile)

add_library(thread_lib INTERFACE)
set(AIO_FLAGS)

## Look for the C11 and later standard concurrency library:

set(C11_CONCURRENCY_LIB FALSE)
set(C11_STANDARD_ATOMIC FALSE)

if (WITH_ASYNC)
    file(WRITE
        ${CMAKE_BINARY_DIR}/CMakeTmp/testc11atomic.c
            "#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)\n"
            "   /* C11 or newer compiler -- use the compiler's support for atomic types. */\n"
            "#  include <stdatomic.h>\n"
            "#  define HAVE_STD_ATOMIC 1\n"
            "#else\n"
            "#  define HAVE_STD_ATOMIC 0\n"
            "#endif\n"
            "int main(void) { return ((HAVE_STD_ATOMIC) ? 0 : 1); }\n\n"
    )

    try_run(RUN_C11STD COMPILE_C11STD
        SOURCES
            ${CMAKE_BINARY_DIR}/CMakeTmp/testc11atomic.c
        RUN_OUTPUT_VARIABLE THE_C11STD
    )

    if (RUN_C11STD EQUAL 0)
        ## C11+ standard atomic variable support:
        message(STATUS "C11 and later standard atomic variables")
        set(C11_STANDARD_ATOMIC TRUE)
    endif()

    file(WRITE
        ${CMAKE_BINARY_DIR}/CMakeTmp/testc11thrd.c
        "#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)\n"
        "#  include <threads.h>\n"
        "#  define HAVE_STD_THREADS 1\n"
        "#else\n"
        "#  define HAVE_STD_THREADS 0\n"
        "#endif\n"
        "int main(void) { return ((HAVE_STD_THREADS) ? 0 : 1); }\n\n"
    )

    try_run(RUN_C11STD COMPILE_C11STD
        SOURCES
            ${CMAKE_BINARY_DIR}/CMakeTmp/testc11thrd.c
        RUN_OUTPUT_VARIABLE THE_C11STD
    )

    if (NOT COMPILE_C11STD AND (WIN32 OR MINGW))
        ## The MinGW gcc and clang don't ship with a threads.h, so ensure that
        ## sim_threads.h doesn't accidentally think that it exists.
        check_include_file(threads.h C11_STD_THREADS_H)
        if (NOT C11_STD_THREADS_H)
            target_compile_definitions(
                thread_lib
                INTERFACE
                    __STDC_NO_THREADS__
            )
        endif ()
    elseif (RUN_C11STD EQUAL 0)
        ## C11+ standard concurrency library support:
        message(STATUS "C11 and later standard concurrency library")
        set(C11_CONCURRENCY_LIB TRUE)
    endif()

    unset(RUN_C11STD)
    unset(COMPILE_C11STD)
    unset(THE_C11STD)

    include(ExternalProject)

    if (MSVC OR (WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "Clang" AND NOT DEFINED ENV{MSYSTEM}))
        # Pthreads4w: pthreads for windows.
        if (USING_VCPKG)
            find_package(PThreads4W REQUIRED)
            target_link_libraries(thread_lib INTERFACE PThreads4W::PThreads4W)
            set(THREADING_PKG_STATUS "vcpkg PThreads4W")
        else ()
            find_package(PTW)

            if (PTW_FOUND)
                target_compile_definitions(thread_lib INTERFACE PTW32_STATIC_LIB)
                target_include_directories(thread_lib INTERFACE ${PTW_INCLUDE_DIRS})
                target_link_libraries(thread_lib INTERFACE ${PTW_C_LIBRARY})

                set(THREADING_PKG_STATUS "detected PTW/PThreads4W")
            else ()
                ## Would really like to build from the original jwinarske repo, but it
                ## ends up installing in ${CMAKE_INSTALL_PREFIX}/<x86|x86_64>> prefix.
                ## Which completely breaks how CMake Find*.cmake works.
                ##
                ## set(PTHREADS4W_URL "https://github.com/jwinarske/pthreads4w")
                ## set(PTHREADS4W_URL "https://github.com/bscottm/pthreads4w")
                set(PTHREADS4W_URL "https://github.com/bscottm/pthreads4w/archive/refs/tags/version-3.1.0-release.zip")

                ExternalProject_Add(pthreads4w-ext
                    URL ${PTHREADS4W_URL}
                    CONFIGURE_COMMAND ""
                    BUILD_COMMAND ""
                    INSTALL_COMMAND ""
                )

                BuildDepMatrix(pthreads4w-ext pthreads4w
                    # CMAKE_ARGS
                    #     -DDIST_ROOT=${SIMH_DEP_TOPDIR}
                )

                list(APPEND SIMH_BUILD_DEPS pthreads4w)
                list(APPEND SIMH_DEP_TARGETS pthreads4w-ext)
                message(STATUS "Building Pthreads4w from ${PTHREADS4W_URL}")
                set(THREADING_PKG_STATUS "pthreads4w source build")
            endif ()
        endif ()
    else ()
        # Let CMake determine which threading library ought be used.
        set(THREADS_PREFER_PTHREAD_FLAG On)
        find_package(Threads)
        if (THREADS_FOUND)
          target_link_libraries(thread_lib INTERFACE Threads::Threads)
        endif (THREADS_FOUND)

        set(THREADING_PKG_STATUS "Platform-detected threading support")
    endif ()

    if (THREADS_FOUND OR PTW_FOUND OR PThreads4W_FOUND)
        set(AIO_FLAGS USE_READER_THREAD SIM_ASYNCH_IO)

        ## CMAKE_USE_PTHREADS_INIT: See CMake's FindThreads package documentation.
        if (PTW_FOUND OR PThreads4W_FOUND OR CMAKE_USE_PTHREADS_INIT)
            target_compile_definitions(
                thread_lib
                INTERFACE
                    USING_PTHREADS=$<IF:$<BOOL:${C11_CONCURRENCY_LIB}>,0,1>
            )
        endif ()
    else ()
        set(AIO_FLAGS DONT_USE_READER_THREAD)
    endif ()
else()
    target_compile_definitions(thread_lib INTERFACE DONT_USE_READER_THREAD)
    set(THREADING_PKG_STATUS "asynchronous I/O disabled.")
endif()
