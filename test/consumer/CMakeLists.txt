cmake_minimum_required(VERSION 3.16)
project(fmpool_consumer LANGUAGES C)
find_package(fmpool CONFIG REQUIRED)
enable_testing()

foreach(variant IN ITEMS single_thread thread_safe)
    add_executable(consumer_${variant} main.c producer.c)
    target_link_libraries(consumer_${variant} PRIVATE fmpool::${variant})
    add_test(NAME consumer.${variant} COMMAND consumer_${variant})
    get_target_property(library_type fmpool::${variant} TYPE)
    if(WIN32 AND library_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET consumer_${variant} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:fmpool::${variant}>
                $<TARGET_FILE_DIR:consumer_${variant}>)
    endif()
endforeach()
