cmake_minimum_required(VERSION 3.20)
project(RolsTraker CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(third_party/ftxui)

add_executable(RolsTraker src/main.cpp src/ftxui_image.cpp)

target_link_libraries(RolsTraker PRIVATE ftxui::screen ftxui::dom ftxui::component winhttp ws2_32)
