set(TARGET_NAME ${SHADER_COMPILE_TARGET})

file(
    GLOB_RECURSE
    SHADER_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.geom"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.tesc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.tese"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mesh"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.task"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rgen"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rchit"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rmiss"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rcall"
)

SOURCE_GROUP("shaders" FILES ${SHADER_FILES})

if(NOT EXISTS ${SHADER_GEN_DIR})
    file(MAKE_DIRECTORY ${SHADER_GEN_DIR})
endif()

if(NOT EXISTS ${SHADER_GEN_DIR}/spv)
  file(MAKE_DIRECTORY ${SHADER_GEN_DIR}/spv)
endif()

include(${VAIN_ROOT_DIR}/cmake/ShaderCompile.cmake)

compile_shader(
  "${SHADER_FILES}"
  "${TARGET_NAME}"
  "."
  "${SHADER_GEN_DIR}"
  "${glslangValidator_executable}"
)

set_target_properties("${TARGET_NAME}" PROPERTIES FOLDER "Engine" )