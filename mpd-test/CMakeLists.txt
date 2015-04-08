cmake_minimum_required(VERSION 2.6)

project (mpdtest)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "2")
set(STREAMRIPPER_LIB_PATH "${PROJECT_SOURCE_DIR}/../../../libstreamripper/")

find_package(LibMPDClient REQUIRED)
find_package(Threads REQUIRED)
find_package(GLIB2 REQUIRED)

include_directories(${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR} ${GLIB2_INCLUDE_DIRS})

include(CheckCSourceCompiles)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall -D__UNIX__")
set(CMAKE_C_FLAGS_DEBUG "-ggdb -pedantic")

get_target_property(MKDATA_EXE mkdata LOCATION)

set(SOURCES
    src/ympd.c
)

add_executable(mpdtest ${SOURCES})
target_link_libraries(mpdtest ${CMAKE_THREAD_LIBS_INIT} ${LIBMPDCLIENT_LIBRARY})

install(TARGETS mpdtest DESTINATION bin)
