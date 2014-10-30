cmake_minimum_required(VERSION 2.6)

project (srtest)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "2")
set(STREAMRIPPER_LIB_PATH "${PROJECT_SOURCE_DIR}/../../../libstreamripper/")

find_package(Threads REQUIRED)
find_package(GLIB2 REQUIRED)

include_directories(${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR} ${STREAMRIPPER_LIB_PATH} ${GLIB2_INCLUDE_DIRS})

include(CheckCSourceCompiles)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall -D__UNIX__") 
set(CMAKE_C_FLAGS_DEBUG "-ggdb -pedantic")

get_target_property(MKDATA_EXE mkdata LOCATION)

set(SOURCES
    src/ympd.c
    src/streamripper.c
)

add_executable(srtest ${SOURCES})
target_link_libraries(srtest ${CMAKE_THREAD_LIBS_INIT} streamripper mad ogg vorbis)

install(TARGETS srtest DESTINATION bin)
