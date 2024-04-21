#version 460

#extension GL_GOOGLE_include_directive: enable

#include "inc/constants.h"
#include "inc/gbuffer.h"

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

layout(set = 2, binding = 1) uniform samplerCube skybox_sampler;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput in_gbuffer_a;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput in_gbuffer_b;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput in_gbuffer_c;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput in_scene_depth;

layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

#include "inc/mesh_lighting.h"

void main() {
    PGBufferData gbuffer;
    vec4 gbuffer_a = subpassLoad(in_gbuffer_a).rgba;
    vec4 gbuffer_b = subpassLoad(in_gbuffer_b).rgba;
    vec4 gbuffer_c = subpassLoad(in_gbuffer_c).rgba;
    decodeGBufferData(gbuffer, gbuffer_a, gbuffer_b, gbuffer_c);

    vec3  N                   = gbuffer.world_normal;
    vec3  base_color          = gbuffer.base_color;
    float metallic            = gbuffer.metallic;
    float dielectric_specular = 0.08 * gbuffer.specular;
    float roughness           = gbuffer.roughness;

    vec3 in_world_position;
    {
        float scene_depth              = subpassLoad(in_scene_depth).r;
        vec4  ndc                      = vec4(uv_to_ndcxy(in_texcoord), scene_depth, 1.0);
        mat4  inverse_proj_view_matrix = inverse(proj_view_matrix);
        vec4  in_world_position_with_w = inverse_proj_view_matrix * ndc;
        in_world_position              = in_world_position_with_w.xyz / in_world_position_with_w.w;
    }

    vec3 result_color = vec3(0.0, 0.0, 0.0);

    if (gbuffer.shading_model_id == SHADING_MODEL_ID_UNLIT) {
        vec3 in_uvw = normalize(in_world_position - camera_position);
        result_color = textureLod(skybox_sampler, in_uvw, 0.0).rgb;
    } else if (gbuffer.shading_model_id == SHADING_MODEL_ID_DEFAULT_LIT) {
        #include "inc/mesh_lighting.inl"
    }

    out_color = vec4(result_color, 1.0);
}