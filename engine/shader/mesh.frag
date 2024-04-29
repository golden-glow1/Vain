#version 460

#extension GL_GOOGLE_include_directive: enable

#include "inc/constants.h"

struct DirectionalLight {
    vec3  direction;
    float _padding_direction;
    vec3  color;
    float _padding_color;
};

struct PointLight {
    vec3  position;
    float radius;
    vec3  intensity;
    float _padding_intensity;
};

layout(set = 0, binding = 0) readonly buffer _per_frame {
    mat4             proj_view_matrix;
    vec3             camera_position;
    float            _padding_camera_position;
    vec3             ambient_light;
    float            _padding_ambient_light;
    uint             point_light_num;
    uint             _padding_point_light_num_1;
    uint             _padding_point_light_num_2;
    uint             _padding_point_light_num_3;
    PointLight       scene_point_lights[max_point_light_count];
    DirectionalLight scene_directional_light;
    mat4             directional_light_proj_view;
};

layout(set = 0, binding = 2) uniform sampler2D brdfLUT_sampler;
layout(set = 0, binding = 3) uniform samplerCube irradiance_sampler;
layout(set = 0, binding = 4) uniform samplerCube specular_sampler;
layout(set = 0, binding = 5) uniform sampler2DArray point_lights_shadow;
layout(set = 0, binding = 6) uniform sampler2D directional_light_shadow;

layout(set = 1, binding = 0) uniform _per_material {
    vec4  base_color_factor;
    float metallic_factor;
    float roughness_factor;
    float normal_scale;
    float occlusion_strength;
    vec3  emissive_factor;
    uint  is_blend;
    uint  is_double_sided;
};

layout(set = 1, binding = 1) uniform sampler2D base_color_texture_sampler;
layout(set = 1, binding = 2) uniform sampler2D metallic_roughness_texture_sampler;
layout(set = 1, binding = 3) uniform sampler2D normal_texture_sampler;
layout(set = 1, binding = 4) uniform sampler2D occlusion_texture_sampler;
layout(set = 1, binding = 5) uniform sampler2D emissive_color_texture_sampler;

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_texcoord;

layout(location = 0) out vec4 out_scene_color;

vec3 getBaseColor() {
    vec3 base_color = texture(base_color_texture_sampler, in_texcoord).xyz * base_color_factor.xyz;
    return base_color;
}

vec3 calculateNormal() {
    vec3 tangent_normal = texture(normal_texture_sampler, in_texcoord).xyz * 2.0 - 1.0;

    vec3 N = in_normal;
    vec3 T = in_tangent;
    vec3 B = normalize(cross(N, T));

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangent_normal);
}

#include "inc/mesh_lighting.h"

void main() {
    vec3  N                   = calculateNormal();
    vec3  base_color          = getBaseColor();
    float metallic            = texture(metallic_roughness_texture_sampler, in_texcoord).b * metallic_factor;
    float dielectric_specular = 0.04;
    float roughness           = texture(metallic_roughness_texture_sampler, in_texcoord).g * roughness_factor;

    vec3 result_color;

    #include "inc/mesh_lighting.inl"

    out_scene_color = vec4(result_color, 1.0);
}