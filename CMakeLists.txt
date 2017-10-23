cmake_minimum_required(VERSION 2.6)
project(picam2fits)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release (-DCMAKE_BUILD_TYPE=...)")
    set(CMAKE_BUILD_TYPE "Release")
endif()

find_package(CFITSIO REQUIRED)

include_directories(${CFITSIO_INCLUDES})

add_executable(picam2fits picam2fits.cpp)
target_link_libraries(picam2fits ${CFITSIO_LIBRARY})

install(TARGETS picam2fits DESTINATION bin)
