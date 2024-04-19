#version 460

#extension GL_GOOGLE_include_directive: enable
#extension GL_EXT_geometry_shader: enable

#include "inc/constants.h"

layout(set = 0, binding = 0) readonly buffer _per_frame {
    uint point_light_count;
    uint _padding_point_light_count_0;
    uint _padding_point_light_count_1;
    uint _padding_point_light_count_2;
    vec4 point_lights_position_and_radius[max_point_light_count];
};

layout(triangles) in;
layout(triangle_strip, max_vertices = max_point_light_geom_vertices) out;

layout(location = 0) in vec3 in_positions_world_space[];

layout(location = 0) out float out_inv_length;
layout(location = 1) out vec3 out_inv_length_position_view_space;

void main() {
    for (
        int point_light_index = 0;
        point_light_index < point_light_count && point_light_index < max_point_light_count;
        ++point_light_index
    ) {
        vec3 point_light_position = point_lights_position_and_radius[point_light_index].xyz;
        float point_light_radius = point_lights_position_and_radius[point_light_index].w;

        for (int layer_index = 0; layer_index < 2; ++layer_index) {
            for (int vertex_index = 0; vertex_index < 3; ++vertex_index) {
                vec3 position_world_space = in_positions_world_space[vertex_index];

                vec3 position_view_space = position_world_space - point_light_position;

                vec3 position_spherical_function_domain = normalize(position_view_space);

                float layer_position_spherical_function_domain_z[2];
                layer_position_spherical_function_domain_z[0] = -position_spherical_function_domain.z;
                layer_position_spherical_function_domain_z[1] = position_spherical_function_domain.z;
                vec4 position_clip;
                position_clip.xy = position_spherical_function_domain.xy;
                position_clip.w = layer_position_spherical_function_domain_z[layer_index] + 1.0;
                position_clip.z = length(position_view_space) * position_clip.w / point_light_radius;;
                gl_Position = position_clip;

                out_inv_length = 1.0f / length(position_view_space);
                out_inv_length_position_view_space = out_inv_length * position_view_space;

                gl_Layer = layer_index + 2 * point_light_index;
                EmitVertex();
            }
            EndPrimitive();
        }
    }
}