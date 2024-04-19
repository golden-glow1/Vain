set(ENGINE_ROOT_DIR "${VAIN_ROOT_DIR}/engine")
set(THIRD_PARTY_DIR "${ENGINE_ROOT_DIR}/3rdparty")
set(ENGINE_ASSET_DIR "asset")
set(ENGINE_CONFIG_DIR "config")

find_package(Vulkan REQUIRED)
set(vulkan_lib Vulkan::Vulkan)

if(WIN32)
    set(glslangValidator_executable $ENV{VULKAN_SDK}/Bin/glslangValidator.exe)
else()
    set(glslangValidator_executable glslangValidator)
endif()

set(SHADER_COMPILE_TARGET VainShaderCompile)
set(SHADER_GEN_DIR ${CMAKE_BINARY_DIR}/shaders)
add_subdirectory(shader)

add_subdirectory(3rdparty)

add_subdirectory(source/runtime)
add_subdirectory(source/editor)