add_executable(perftest perftest.c)
target_link_libraries(perftest PRIVATE fmpool::fmpool)

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET perftest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmpool_st> $<TARGET_FILE:fmpool_mt>
            $<TARGET_FILE_DIR:perftest>)
endif()

foreach(variant IN ITEMS st mt)
    add_executable(perftest_${variant} variantbench.c)
    target_link_libraries(perftest_${variant} PRIVATE fmpool_${variant})
    if(NOT WIN32)
        target_link_libraries(perftest_${variant} PRIVATE Threads::Threads)
    endif()
    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET perftest_${variant} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:fmpool_${variant}>
                $<TARGET_FILE_DIR:perftest_${variant}>)
    endif()
endforeach()

add_custom_target(benchmark_compare
    COMMAND ${CMAKE_COMMAND}
        -DST_EXECUTABLE=$<TARGET_FILE:perftest_st>
        -DMT_EXECUTABLE=$<TARGET_FILE:perftest_mt>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/performance.cmake
    DEPENDS perftest_st perftest_mt
    USES_TERMINAL)

if(BUILD_TESTING)
    add_test(NAME fmpool.performance.compare
        COMMAND ${CMAKE_COMMAND}
            -DST_EXECUTABLE=$<TARGET_FILE:perftest_st>
            -DMT_EXECUTABLE=$<TARGET_FILE:perftest_mt>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/performance.cmake)
    add_test(NAME fmpool.performance.st.1thread COMMAND perftest_st --threads 1)
    add_test(NAME fmpool.performance.mt.1thread COMMAND perftest_mt --threads 1)
    add_test(NAME fmpool.performance.mt.4threads COMMAND perftest_mt --threads 4)
    add_test(NAME fmpool.performance.st.4threads.independent
        COMMAND perftest_st --threads 4 --independent-pools)
    set_tests_properties(fmpool.performance.compare
        fmpool.performance.st.1thread fmpool.performance.mt.1thread
        fmpool.performance.mt.4threads fmpool.performance.st.4threads.independent PROPERTIES
        LABELS performance RUN_SERIAL TRUE TIMEOUT 300)
endif()
