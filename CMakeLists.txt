cmake_minimum_required(VERSION 3.28)
project(sigcraft)

set(CMAKE_CXX_STANDARD 20)

set(IMR_FOLDER "imr" CACHE STRING "Folder where IMR is found")
add_subdirectory(${IMR_FOLDER} imr_build)

if (NOT TARGET imr)
    message("IMR not found. Please set IMR_FOLDER accordingly.")
endif ()

add_subdirectory(enklume)

add_executable(sigcraft main.cpp camera.cpp chunk_mesh.cpp world.cpp voxel.cpp)
target_link_libraries(sigcraft imr enklume nasl::nasl)

add_custom_target(basic_vert_spv COMMAND ${GLSLANG_EXE} -V -S vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.vert -o ${CMAKE_CURRENT_BINARY_DIR}/basic.vert.spv)
add_dependencies(sigcraft basic_vert_spv)
add_custom_target(basic_frag_spv COMMAND ${GLSLANG_EXE} -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.frag -o ${CMAKE_CURRENT_BINARY_DIR}/basic.frag.spv)
add_dependencies(sigcraft basic_frag_spv)

add_custom_target(voxel_vert_spv COMMAND ${GLSLANG_EXE} -V -S vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/voxel.vert -o ${CMAKE_CURRENT_BINARY_DIR}/voxel.vert.spv)
add_dependencies(sigcraft voxel_vert_spv)
add_custom_target(voxel_frag_spv COMMAND ${GLSLANG_EXE} -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/voxel.frag -o ${CMAKE_CURRENT_BINARY_DIR}/voxel.frag.spv)
add_dependencies(sigcraft voxel_frag_spv)