cmake_minimum_required(VERSION 3.20)
project(RolsTracker CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(third_party/ftxui)

add_executable(RolsTracker src/main.cpp app.rc)

target_link_libraries(RolsTracker PRIVATE ftxui::screen ftxui::dom ftxui::component winhttp ws2_32 crypt32 ole32 uuid shell32)

if(WIN32)
    target_link_options(RolsTracker PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_custom_command(TARGET RolsTracker POST_BUILD
    COMMAND cmd /c copy /y "$<TARGET_FILE:RolsTracker>" "${CMAKE_CURRENT_SOURCE_DIR}\\RolsTracker.exe" 2>nul || exit 0
    COMMAND cmd /c copy /y "$<TARGET_FILE:RolsTracker>" "${CMAKE_CURRENT_SOURCE_DIR}\\RolsTraker.exe" 2>nul || exit 0
    COMMENT "Syncing RolsTracker.exe and RolsTraker.exe to project root"
)
