cmake_minimum_required(VERSION 3.20)
project(HamsCubeGame VERSION 0.01 LANGUAGES C CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(hams-cube-game WIN32 src/main.cpp src/font_picker.cpp src/winmain.cpp)
target_compile_features(hams-cube-game PRIVATE cxx_std_20)
target_link_libraries(hams-cube-game PRIVATE raylib)
if(WIN32)
    target_link_libraries(hams-cube-game PRIVATE comdlg32)
endif()
if(MSVC)
    target_compile_options(hams-cube-game PRIVATE /W4 /permissive-)
else()
    target_compile_options(hams-cube-game PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET hams-cube-game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:hams-cube-game>/assets"
)

