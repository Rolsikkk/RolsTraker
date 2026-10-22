cmake_minimum_required(VERSION 3.20)
project(RolsTraker CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(third_party/ftxui)

add_executable(RolsTraker src/main.cpp app.rc)

target_link_libraries(RolsTraker PRIVATE ftxui::screen ftxui::dom ftxui::component winhttp ws2_32 crypt32 shell32)

if(WIN32)
    target_link_options(RolsTraker PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_custom_command(TARGET RolsTraker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:RolsTraker> "${CMAKE_CURRENT_SOURCE_DIR}/RolsTraker.exe"
    COMMENT "Syncing RolsTraker.exe to project root"
)
