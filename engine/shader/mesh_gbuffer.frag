#version 460

#extension GL_GOOGLE_include_directive: enable

#include "inc/gbuffer.h"

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
layout(set = 1, binding = 2) uniform sampler2D normal_texture_sampler;
layout(set = 1, binding = 3) uniform sampler2D metallic_texture_sampler;
layout(set = 1, binding = 4) uniform sampler2D roughness_texture_sampler;
layout(set = 1, binding = 5) uniform sampler2D occlusion_texture_sampler;
layout(set = 1, binding = 6) uniform sampler2D emissive_color_texture_sampler;

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_texcoord;

layout(location = 0) out vec4 out_gbuffer_a;
layout(location = 1) out vec4 out_gbuffer_b;
layout(location = 2) out vec4 out_gbuffer_c;

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

void main() {
    PGBufferData gbuffer;
    gbuffer.world_normal     = calculateNormal();
    gbuffer.base_color       = getBaseColor();
    gbuffer.metallic         = texture(metallic_texture_sampler, in_texcoord).x * metallic_factor;
    gbuffer.specular         = 0.5;
    gbuffer.roughness        = texture(roughness_texture_sampler, in_texcoord).x * roughness_factor;
    gbuffer.shading_model_id = SHADING_MODEL_ID_DEFAULT_LIT;

    encodeGBufferData(gbuffer, out_gbuffer_a, out_gbuffer_b, out_gbuffer_c);
}