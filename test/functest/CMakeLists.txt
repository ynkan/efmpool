foreach(variant IN ITEMS st mt)
    foreach(name IN ITEMS functest growthtest)
        add_executable(${name}_${variant} ${name}.c)
        target_link_libraries(${name}_${variant} PRIVATE fmpool_${variant})
        add_test(NAME fmpool.${name}.${variant} COMMAND ${name}_${variant})
    endforeach()
endforeach()

add_executable(threadtest threadtest.c)
target_link_libraries(threadtest PRIVATE fmpool::thread_safe)
add_test(NAME fmpool.threaded COMMAND threadtest)

# Keep shared-library dependencies next to executables on Windows.
if(WIN32 AND BUILD_SHARED_LIBS)
    foreach(name IN ITEMS functest_st growthtest_st functest_mt growthtest_mt threadtest)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:fmpool_st> $<TARGET_FILE:fmpool_mt>
                $<TARGET_FILE_DIR:${name}>)
    endforeach()
endif()
