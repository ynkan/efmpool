cmake_minimum_required(VERSION 3.16)
project(fmpool VERSION 2.0.0 LANGUAGES C)

option(FMPOOL_THREAD_SAFE "Use the thread-safe library for the default target" OFF)
option(FMPOOL_CHECKS "Validate ownership and reject double frees" ON)
option(FMPOOL_BUILD_EXAMPLES "Build the example" ON)
option(FMPOOL_BUILD_BENCHMARKS "Build the benchmark" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries instead of static libraries" OFF)
include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT WIN32)
    find_package(Threads REQUIRED)
endif()

function(fmpool_add_library target thread_safe export_name)
    add_library(${target} fmpool.c)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_features(${target} PUBLIC c_std_99)
    target_compile_definitions(${target} PUBLIC
        FMPOOL_THREAD_SAFE=${thread_safe}
        FMPOOL_CHECKS=$<BOOL:${FMPOOL_CHECKS}>)
    target_compile_definitions(${target} PRIVATE FMPOOL_BUILDING_LIBRARY)
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PUBLIC FMPOOL_SHARED)
    endif()
    if(thread_safe AND NOT WIN32)
        target_link_libraries(${target} PUBLIC Threads::Threads)
    endif()
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${export_name})
endfunction()

fmpool_add_library(fmpool_st 0 single_thread)
fmpool_add_library(fmpool_mt 1 thread_safe)
add_library(fmpool::single_thread ALIAS fmpool_st)
add_library(fmpool::thread_safe ALIAS fmpool_mt)

add_library(fmpool INTERFACE)
add_library(fmpool::fmpool ALIAS fmpool)
if(FMPOOL_THREAD_SAFE)
    target_link_libraries(fmpool INTERFACE fmpool_mt)
else()
    target_link_libraries(fmpool INTERFACE fmpool_st)
endif()

install(TARGETS fmpool_st fmpool_mt fmpool EXPORT fmpoolTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES fmpool.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT fmpoolTargets NAMESPACE fmpool::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fmpool)
configure_package_config_file(cmake/fmpoolConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/fmpoolConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fmpool)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/fmpoolConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fmpoolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/fmpoolConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fmpool)

if(BUILD_TESTING OR FMPOOL_BUILD_EXAMPLES OR FMPOOL_BUILD_BENCHMARKS)
    add_subdirectory(test)
endif()
