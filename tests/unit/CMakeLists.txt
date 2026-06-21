add_executable(litho_test
    test_main.cpp
    test_region.cpp
    test_painter.cpp
    test_dirty_list.cpp
    test_pfb.cpp
    test_rotate.cpp
)

target_include_directories(litho_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(litho_test litho)
