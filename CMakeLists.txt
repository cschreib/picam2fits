cmake_minimum_required(VERSION 2.6)
project(picam2fits)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release (-DCMAKE_BUILD_TYPE=...)")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-std=c++11)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-std=c++11)
else()
    message(FATAL_ERROR "compilers other than clang and gcc are not supported")
endif()

find_package(CFITSIO REQUIRED)

include_directories(${CFITSIO_INCLUDES})

add_executable(picam2fits picam2fits.cpp)
target_link_libraries(picam2fits ${CFITSIO_LIBRARIES})

install(TARGETS picam2fits DESTINATION bin)
