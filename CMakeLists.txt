cmake_minimum_required(VERSION 3.16)
project(LithoUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(X11 REQUIRED)

# LithoUI framework library
add_library(litho STATIC
    framework/view/view.cpp
    framework/intent/bundle.cpp
    framework/activity/activity.cpp
)

target_include_directories(litho PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(litho PUBLIC ${X11_LIBRARIES})
target_include_directories(litho PUBLIC ${X11_INCLUDE_DIR})

add_subdirectory(apps/hello_litho)
add_subdirectory(tests/unit)
