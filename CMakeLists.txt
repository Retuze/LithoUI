cmake_minimum_required(VERSION 3.16)
project(LithoUI LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- resource images: PNG → bin → embedded .o ----

# ── resource images ──
# UI directory layout: ui/<name>/{info.txt, rgb565/, rgba/, grayscale/, rotatable/}

set(UI_DIR    "${CMAKE_SOURCE_DIR}/ui/hello_litho")
set(GEN_DIR   "${CMAKE_BINARY_DIR}/generated")
set(RES_BIN   "${GEN_DIR}/res_images.bin")
set(RES_HDR   "${GEN_DIR}/res_images.h")
set(RES_OBJ   "${GEN_DIR}/res_images.o")

file(GLOB_RECURSE RES_PNG_FILES "${UI_DIR}/*.png")

# Step 1: pack PNG → res_images.bin + res_images.h
add_custom_command(
    OUTPUT ${RES_BIN} ${RES_HDR}
    COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/pack_res.py"
            "${UI_DIR}" "${GEN_DIR}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/pack_res.py" ${RES_PNG_FILES}
    COMMENT "Packing ${UI_DIR} → res_images.bin + headers"
)

# Step 2: embed bin as linkable object in .rodata (read-only).
# Must run in the bin's directory so the symbol names are clean:
#   _binary_res_images_bin_start / _end / _size
add_custom_command(
    OUTPUT ${RES_OBJ}
    COMMAND objcopy -I binary -O elf64-littleaarch64
            --rename-section .data=.rodata,contents,alloc,load,readonly
            res_images.bin res_images.o
    DEPENDS ${RES_BIN}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/generated
    COMMENT "Embedding res_images.bin → res_images.o (.rodata)"
)

# Convenience target for manual re-pack
add_custom_target(pack_res
    COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/pack_res.py"
            "${UI_DIR}" "${GEN_DIR}"
    COMMENT "Packing ${UI_DIR} → generated/"
)

# ---- LithoUI framework library ----

add_library(litho STATIC
    framework/view/view.cpp
    framework/intent/bundle.cpp
    framework/activity/activity.cpp
    ${RES_OBJ}
)

target_include_directories(litho PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

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
