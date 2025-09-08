cmake_minimum_required(VERSION 3.16)
project(WebServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    main.cpp
    core/server.cpp
    core/logging.cpp
    http/httpreq.cpp
    utils/filehandle.cpp
)

add_executable(webserver ${SOURCES})

target_include_directories(webserver PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/http
    ${CMAKE_SOURCE_DIR}/utils
)

if(WIN32)
    target_link_libraries(webserver PRIVATE ws2_32)
endif()


