## Unity test framework library
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

##-~-~
## Atomic primitives test
add_executable(test_atomic
    test_atomic.c
    ${CMAKE_SOURCE_DIR}/sim_atomic.c
)

set_target_properties(
    test_atomic
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SIMH_LEGACY_INSTALL}/unittests"
)

target_include_directories(
    test_atomic
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(
    test_atomic
    PUBLIC
        unity_framework
        os_features
        thread_lib
)

add_test(NAME test_atomic COMMAND test_atomic)

file(WRITE
    ${CMAKE_BINARY_DIR}/CMakeTmp/testc11std.c
    "#if !defined(__STDC_NO_ATOMICS__) && __STDC_VERSION__ >= 201112L\n"
    "/* C11 or newer compiler -- use the compiler's support for atomic types. */\n"
    "#include <stdatomic.h>\n"
    "#define HAVE_STD_ATOMIC 1\n"
    "#else\n"
    "#define HAVE_STD_ATOMIC 0\n"
    "#endif\n"
    "int main(void) { return (HAVE_STD_ATOMIC ? 0 : 1); }\n"
)

try_run(RUN_C11STD COMPILE_C11STD
    SOURCES ${CMAKE_BINARY_DIR}/CMakeTmp/testc11std.c
    RUN_OUTPUT_VARIABLE THE_C11STD
)

message(STATUS "CMAKE_C_COMPILER_ID ${CMAKE_C_COMPILER_ID}")
if (RUN_C11STD EQUAL 0 AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
    message(STATUS "C11 and later standard atomics, test_atomic_nostd unit test.")
    add_executable(test_atomic_nostd
        test_atomic.c
        ${CMAKE_SOURCE_DIR}/sim_atomic.c
    )

    set_target_properties(
        test_atomic_nostd
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${SIMH_LEGACY_INSTALL}/unittests"
    )

    target_compile_definitions(
        test_atomic_nostd
        PRIVATE
            __STDC_NO_ATOMICS__   
    )

    target_include_directories(
        test_atomic_nostd
        PUBLIC
            "${CMAKE_SOURCE_DIR}"
    )

    target_link_libraries(
        test_atomic_nostd
        PUBLIC
            unity_framework
            os_features
            thread_lib
    )
endif ()

unset(RUN_C11STD)
unset(COMPILE_C11STD)
