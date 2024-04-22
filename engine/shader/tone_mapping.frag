#version 460

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput in_color;

layout(location = 0) out vec4 out_color;

vec3 uncharted2Tonemap(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

void main() {
    vec3 color = subpassLoad(in_color).rgb;
    color = uncharted2Tonemap(color * 4.5f);
    color = color * (1.0f / uncharted2Tonemap(vec3(11.2f)));    
    out_color = vec4(color, 1.0);
}