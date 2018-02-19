cmake_minimum_required(VERSION 3.9)
project(ZPack)
enable_testing()

#set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install-${CMAKE_BUILD_TYPE}/")
include_directories("${PROJECT_BINARY_DIR}")

MESSAGE(STATUS "INSTALL PREFIX: " ${CMAKE_INSTALL_PREFIX})

find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package(GTest REQUIRED)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release" OR "${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    set(ZPACK_DEBUG false)
else ()
    set(ZPACK_DEBUG true)
endif ()

configure_file(
        "${PROJECT_SOURCE_DIR}/_cfg.in.h"
        "${PROJECT_BINARY_DIR}/_cfg.h")

set(FILES_SRC
        zpack.cpp
        _endianness.cpp
        zpack_zstd.cpp
        zpack_compression.cpp)

set(FILES_HDR
        zpack.h
        _endianness.h
        zpack_zstd.h
        zpack_compression.h
        _prepare_int.h)

set(LINK_TARGETS
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        zstd)

add_library(libzstd STATIC IMPORTED)

add_executable(ZPack main.cpp ${FILES_SRC} ${FILES_HDR})
target_link_libraries(ZPack ${LINK_TARGETS})
#set_target_properties(ZPack PROPERTIES COMPILE_FLAGS "-save-temps")

add_executable(ZPackd main_test.cpp ${FILES_SRC} ${FILES_HDR})
target_link_libraries(ZPackd GTest::GTest GTest::Main ${LINK_TARGETS})

add_library(ZPack_shared SHARED ${FILES_SRC} ${FILES_HDR})
target_link_libraries(ZPack_shared ${LINK_TARGETS})

add_library(ZPack_static STATIC ${FILES_SRC} ${FILES_HDR})
target_link_libraries(ZPack_static ${LINK_TARGETS})

add_executable(ZPack_link_share main_link.cpp ${FILES_HDR})
target_link_libraries(ZPack_link_share ZPack_shared)

add_executable(ZPack_link_static main_link.cpp ${FILES_HDR})
target_link_libraries(ZPack_link_static ZPack_static)

install(TARGETS ZPack ZPack_shared ZPack_static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES zpack.h zpack_compression.h zpack_zstd.h _prepare_int.h _endianness.h ${PROJECT_BINARY_DIR}/_cfg.h
        DESTINATION include)