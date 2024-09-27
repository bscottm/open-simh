
## Atomic primitives test
add_executable(test_atomic
    ${CMAKE_SOURCE_DIR}/tests/test_atomic.c
    ${CMAKE_SOURCE_DIR}/sim_atomic.c
)

set_target_properties(test_atomic PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SIMH_LEGACY_INSTALL}/diags")
target_include_directories(test_atomic PUBLIC "${CMAKE_SOURCE_DIR}")
target_link_libraries(test_atomic PUBLIC os_features thread_lib)
