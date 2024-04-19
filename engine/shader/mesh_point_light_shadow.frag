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

layout(location = 0) in float in_inv_length;
layout(location = 1) in vec3 in_inv_length_position_view_space;

layout(location = 0) out float out_depth;

void main() {
    vec3 position_view_space = in_inv_length_position_view_space / in_inv_length;

    float point_light_radius = point_lights_position_and_radius[gl_Layer / 2].w;

    float ratio = length(position_view_space) / point_light_radius;

    out_depth = ratio;
}