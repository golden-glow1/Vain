#version 460

layout(set = 0, binding = 1) uniform samplerCube specular_sampler;

layout(location = 0) in vec3 in_uvw;

layout(location = 0) out vec4 out_scene_color;

void main() {
    vec3 color = textureLod(specular_sampler, in_uvw, 0.0).rgb;
    out_scene_color = vec4(color, 1.0);
}