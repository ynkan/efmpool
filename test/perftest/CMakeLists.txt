add_executable(perftest perftest.c)
target_link_libraries(perftest PRIVATE fmpool::fmpool)

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET perftest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmpool_st> $<TARGET_FILE:fmpool_mt>
            $<TARGET_FILE_DIR:perftest>)
endif()
