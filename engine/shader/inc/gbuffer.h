struct PGBufferData {
    vec3  world_normal;
    vec3  base_color;
    float metallic;
    float specular;
    float roughness;
    uint  shading_model_id;
};

#define SHADING_MODEL_ID_UNLIT 0U
#define SHADING_MODEL_ID_DEFAULT_LIT 1U

vec3 encodeNormal(vec3 N) { return N * 0.5 + 0.5; }

vec3 decodeNormal(vec3 N) { return N * 2.0 - 1.0; }

vec3 encodeBaseColor(vec3 base_color) {
    // we use sRGB on the render target to give more precision to the darks
    return base_color;
}

vec3 decodeBaseColor(vec3 base_color)
{
    // we use sRGB on the render target to give more precision to the darks
    return base_color;
}

float encodeShadingModelId(uint shading_model_id)
{
    uint value = shading_model_id;
    return (float(value) / float(255));
}

uint decodeShadingModelId(float in_packed_channel) { 
    return uint(round(in_packed_channel * float(255))); 
}

void encodeGBufferData(
    PGBufferData in_gbuffer,
    out vec4 out_gbuffer_a,
    out vec4 out_gbuffer_b,
    out vec4 out_gbuffer_c
) {
    out_gbuffer_a.rgb = encodeNormal(in_gbuffer.world_normal);

    out_gbuffer_b.r = in_gbuffer.metallic;
    out_gbuffer_b.g = in_gbuffer.specular;
    out_gbuffer_b.b = in_gbuffer.roughness;
    out_gbuffer_b.a = encodeShadingModelId(in_gbuffer.shading_model_id);

    out_gbuffer_c.rgb = encodeBaseColor(in_gbuffer.base_color);
}

void decodeGBufferData(
    out PGBufferData out_gbuffer,
    vec4 in_gbuffer_a,
    vec4 in_gbuffer_b, 
    vec4 in_gbuffer_c) {
    out_gbuffer.world_normal = decodeNormal(in_gbuffer_a.xyz);

    out_gbuffer.metallic         = in_gbuffer_b.r;
    out_gbuffer.specular         = in_gbuffer_b.g;
    out_gbuffer.roughness        = in_gbuffer_b.b;
    out_gbuffer.shading_model_id = decodeShadingModelId(in_gbuffer_b.a);

    out_gbuffer.base_color = decodeBaseColor(in_gbuffer_c.rgb);
}