cmake_minimum_required(VERSION 3.28)
project(sigcraft)

set(CMAKE_CXX_STANDARD 20)

set(IMR_FOLDER "imr" CACHE STRING "Folder where IMR is found")
add_subdirectory(${IMR_FOLDER} imr_build)

if (NOT TARGET imr)
    message("IMR not found. Please set IMR_FOLDER accordingly.")
endif ()

add_subdirectory(enklume)

add_executable(sigcraft main.cpp camera.cpp chunk_mesh.cpp world.cpp voxel.cpp game.cpp texture.cpp)
target_link_libraries(sigcraft imr enklume nasl::nasl)

target_include_directories(sigcraft PUBLIC "thirdparty/stb/" "thirdparty/slog/")

add_custom_target(basic_vert_spv COMMAND ${GLSLANG_EXE} -V -S vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.vert -o ${CMAKE_CURRENT_BINARY_DIR}/basic.vert.spv)
add_dependencies(sigcraft basic_vert_spv)
add_custom_target(basic_frag_spv COMMAND ${GLSLANG_EXE} -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.frag -o ${CMAKE_CURRENT_BINARY_DIR}/basic.frag.spv)
add_dependencies(sigcraft basic_frag_spv)

add_custom_target(voxel_vert_spv COMMAND ${GLSLANG_EXE} -V -S vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/voxel.vert -o ${CMAKE_CURRENT_BINARY_DIR}/voxel.vert.spv)
add_dependencies(sigcraft voxel_vert_spv)
add_custom_target(voxel_frag_spv COMMAND ${GLSLANG_EXE} -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/voxel.frag -o ${CMAKE_CURRENT_BINARY_DIR}/voxel.frag.spv)
add_dependencies(sigcraft voxel_frag_spv)
add_custom_target(greedyVoxel_vert_spv COMMAND ${GLSLANG_EXE} -V -S vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/greedyVoxel.vert -o ${CMAKE_CURRENT_BINARY_DIR}/greedyVoxel.vert.spv)
add_dependencies(sigcraft greedyVoxel_vert_spv)

# debug shaders
add_custom_target(debug_billboards_spv COMMAND ${GLSLANG_EXE} -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/visualize_billboards.frag -o ${CMAKE_CURRENT_BINARY_DIR}/visualize_billboards.frag.spv)
add_dependencies(sigcraft debug_billboards_spv)
add_custom_target(outline_billboards_spv COMMAND ${GLSLANG_EXE} -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline_billboards.frag -o ${CMAKE_CURRENT_BINARY_DIR}/outline_billboards.frag.spv)
add_dependencies(sigcraft outline_billboards_spv)
