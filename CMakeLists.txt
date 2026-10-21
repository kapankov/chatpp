cmake_minimum_required(VERSION 3.16)
project(chatpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources contain UTF-8 (Russian) string literals; tell MSVC to treat them as UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Prefer the -pthread compiler flag over plain -lpthread (needed for proper TLS/errno on some toolchains)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(server server.cpp)
add_executable(client client.cpp)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)

# Windows: Winsock2 must be linked explicitly (POSIX needs nothing extra)
if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
    target_link_libraries(client PRIVATE ws2_32)
endif()