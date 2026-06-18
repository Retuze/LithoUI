cmake_minimum_required(VERSION 3.16)
project(LithoUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LithoUI framework library
add_library(litho STATIC
    framework/view/view.cpp
    framework/intent/bundle.cpp
    framework/activity/activity.cpp
)

target_include_directories(litho PUBLIC ${CMAKE_SOURCE_DIR})

if(WIN32)
    # GDI port — no external dependencies needed
    target_compile_definitions(litho PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_subdirectory(apps/hello_litho)
else()
    find_package(X11 REQUIRED)
    target_link_libraries(litho PUBLIC ${X11_LIBRARIES})
    target_include_directories(litho PUBLIC ${X11_INCLUDE_DIR})
    add_subdirectory(apps/hello_litho)
endif()

add_subdirectory(tests/unit)
