cmake_minimum_required(VERSION 3.20)
project(RolsTraker CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(RolsTraker src/main.cpp)

target_link_libraries(RolsTraker PRIVATE winhttp ws2_32)
