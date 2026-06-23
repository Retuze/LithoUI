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

# Step 2: create assembly source that .incbin's the binary.
# Writing it at configure time avoids a separate generation step.
# COFF uses .rdata, ELF uses .rodata for read-only data.
set(RES_EMBED_S "${GEN_DIR}/res_images_embed.S")
if(WIN32)
    file(WRITE ${RES_EMBED_S}
        "    .section .rdata,\"dr\"\n"
        "    .globl _binary_res_images_bin_start\n"
        "    .globl _binary_res_images_bin_end\n"
        "    .balign 1\n"
        "_binary_res_images_bin_start:\n"
        "    .incbin \"res_images.bin\"\n"
        "_binary_res_images_bin_end:\n"
    )
else()
    file(WRITE ${RES_EMBED_S}
        "    .section .rodata\n"
        "    .globl _binary_res_images_bin_start\n"
        "    .globl _binary_res_images_bin_end\n"
        "    .balign 1\n"
        "_binary_res_images_bin_start:\n"
        "    .incbin \"res_images.bin\"\n"
        "_binary_res_images_bin_end:\n"
    )
endif()

# Step 3: assemble embed source → res_images.o.
# Runs in GEN_DIR so that .incbin finds res_images.bin.
add_custom_command(
    OUTPUT ${RES_OBJ}
    COMMAND ${CMAKE_C_COMPILER} -c -o ${RES_OBJ} ${RES_EMBED_S}
    DEPENDS ${RES_BIN} ${RES_EMBED_S}
    WORKING_DIRECTORY ${GEN_DIR}
    COMMENT "Assembling res_images_embed.S → res_images.o"
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

# ---- viz_spin: record one full rotation as a frame sequence (dev tool) ----
add_executable(viz_spin tests/viz_spin.cpp)
target_link_libraries(viz_spin litho)

add_executable(viz_clock tests/viz_clock.cpp)
target_link_libraries(viz_clock litho)

add_executable(viz_clockapp tests/viz_clockapp.cpp)
target_link_libraries(viz_clockapp litho)
