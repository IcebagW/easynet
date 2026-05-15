cmake_minimum_required(VERSION 3.10)
project(easynet VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(easynet
    src/main.cpp
    src/common.h
    src/platform.h
    src/sender.cpp
    src/sender.h
    src/receiver.cpp
    src/receiver.h
    src/tcp.cpp
    src/tcp.h
    src/udp.cpp
    src/udp.h
)

target_include_directories(easynet PRIVATE src)

if(UNIX)
    target_link_libraries(easynet pthread)
elseif(WIN32)
    target_link_libraries(easynet ws2_32)
endif()
