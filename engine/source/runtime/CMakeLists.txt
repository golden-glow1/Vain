set(TARGET_NAME VainRuntime)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Engine")

target_link_libraries(
    ${TARGET_NAME} PUBLIC
    glfw
    glm
    imgui
    rttr_core_lib
    spdlog
    ${vulkan_lib}
    VulkanMemoryAllocator
)

target_link_libraries(
    ${TARGET_NAME} PRIVATE
    stb
    assimp
)

add_dependencies(${TARGET_NAME} ${SHADER_COMPILE_TARGET})
target_include_directories(
    ${TARGET_NAME}
    PRIVATE $<BUILD_INTERFACE:${SHADER_GEN_DIR}/cpp>
)

target_include_directories(
    ${TARGET_NAME} 
    PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source/runtime>
)

target_include_directories(
    ${TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${THIRD_PARTY_DIR}/rapidjson/include>
)
