#version 460

#extension GL_GOOGLE_include_directive: enable

#include "inc/constants.h"
#include "inc/structure.h"

layout(set = 0, binding = 0) readonly buffer _per_frame {
    mat4 light_proj_view;
};

layout(set = 0, binding = 1) readonly buffer _per_drawcall {
    MeshInstance mesh_instances[mesh_per_drawcall_max_instance_count];
};

layout(location = 0) in vec3 in_position;

void main() {
    mat4 model_matrix = mesh_instances[gl_InstanceIndex].model_matrix;

    gl_Position = light_proj_view * model_matrix * vec4(in_position, 1.0);
}