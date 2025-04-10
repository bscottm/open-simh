##-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~
## Unity test framework library
##-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~
add_library(
    unity_framework
    STATIC
        Unity/src/unity.c
)

target_include_directories(
    unity_framework
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Unity/src"
)

##-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~
## Atomic primitives test, if we have a threading library:
##-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~
if (WITH_ASYNC AND (THREADS_FOUND OR PTW_FOUND OR PThreads4W_FOUND))
    ## Minimally, we'll test_atomic (C11 standard or GCC/Clang builtins) and
    ## test_atomic_mtx.
    set(ATOMIC_TESTS "test_atomic" "test_atomic_mtx")

    add_executable(test_atomic
        test_atomic.c
        ${CMAKE_SOURCE_DIR}/sim_atomic.c
    )

    add_executable(test_atomic_mtx
        test_atomic.c
        ${CMAKE_SOURCE_DIR}/sim_atomic.c
    )

    target_compile_definitions(
        test_atomic_mtx
        PUBLIC
        SIM_ATOMIC_MUTEX_ONLY
    )

    file(WRITE
        ${CMAKE_BINARY_DIR}/CMakeTmp/testc11std.c
        "#if !defined(__STDC_NO_ATOMICS__) && __STDC_VERSION__ >= 201112L\n"
        "/* C11 or newer compiler -- use the compiler's support for atomic types. */\n"
        "#include <stdatomic.h>\n"
        "#define HAVE_STD_ATOMIC 1\n"
        "#else\n"
        "#define HAVE_STD_ATOMIC 0\n"
        "#endif\n"
        "int main(void) { return (HAVE_STD_ATOMIC ? 0 : 1); }\n\n"
    )

    try_run(RUN_C11STD COMPILE_C11STD
        SOURCES ${CMAKE_BINARY_DIR}/CMakeTmp/testc11std.c
        RUN_OUTPUT_VARIABLE THE_C11STD
    )

    if (RUN_C11STD EQUAL 0 AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
        message(STATUS "C11 and later standard atomics, test_atomic_nostd unit test.")
        list(APPEND ATOMIC_TESTS "test_atomic_nostd")

        add_executable(test_atomic_nostd
            test_atomic.c
            ${CMAKE_SOURCE_DIR}/sim_atomic.c
        )

        target_compile_definitions(
            test_atomic_nostd
            PRIVATE
                __STDC_NO_ATOMICS__   
        )
    endif ()

    unset(RUN_C11STD)
    unset(COMPILE_C11STD)
    unset(THE_C11STD)

    ## Common settings for the atomic test executables:
    foreach (test_app IN LISTS ATOMIC_TESTS)
        set_target_properties(
            ${test_app}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${SIMH_LEGACY_INSTALL}/unittests"
        )

        target_include_directories(
            ${test_app}
            PUBLIC
                "${CMAKE_SOURCE_DIR}"
        )

        target_link_libraries(
            ${test_app}
            PUBLIC
                unity_framework
                os_features
                thread_lib
        )

        add_test(NAME ${test_app} COMMAND ${test_app})
    endforeach ()

    unset(ATOMIC_TESTS)
endif ()
