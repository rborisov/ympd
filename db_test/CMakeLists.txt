cmake_minimum_required(VERSION 2.6)

project (dbtest)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "2")

find_package(Threads REQUIRED)
find_package(MySQL REQUIRED)

include_directories(${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR} ../src)

include(CheckCSourceCompiles)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall -D__UNIX__") 
set(CMAKE_C_FLAGS_DEBUG "-ggdb -pedantic")
set(CMAKE_BUILD_TYPE debug)
get_target_property(MKDATA_EXE mkdata LOCATION)

set(SOURCES
    src/ympd.c
    ../src/rcar_db.c
)

add_executable(dbtest ${SOURCES})
target_link_libraries(dbtest ${CMAKE_THREAD_LIBS_INIT} mysqlclient)

install(TARGETS dbtest DESTINATION bin)
