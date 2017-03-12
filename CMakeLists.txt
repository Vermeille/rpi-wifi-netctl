CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(wificonf CXX)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -O3 -Wall -Wextra")

add_subdirectory(httpi)

target_include_directories(httpi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(wificonf_src
    )

add_executable(wificonf
    ${wificonf_src}
    src/main.cpp
    )


target_link_libraries(wificonf httpi)
