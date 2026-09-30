add_executable(example example.c)
target_link_libraries(example PRIVATE fmpool::fmpool)

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmpool_st> $<TARGET_FILE:fmpool_mt>
            $<TARGET_FILE_DIR:example>)
endif()
