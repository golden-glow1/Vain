#define PI 3.1416

// todo: param/const
#define MAX_REFLECTION_LOD 8.0

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness) {
    float alpha  = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom  = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
    float r  = (roughness + 1.0);
    float k  = (r * r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// Fresnel function ----------------------------------------------------
float Pow5(float x) {
    return (x * x * x * x * x);
}

vec3 F_Schlick(float cosTheta, vec3 F0)  { 
    return F0 + (1.0 - F0) * Pow5(1.0 - cosTheta); 
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * Pow5(1.0 - cosTheta);
}

// Specular and diffuse BRDF composition --------------------------------------------
vec3 BRDF(vec3  L,
          vec3  V,
          vec3  N,
          vec3  F0,
          vec3  base_color,
          float metallic,
          float roughness
) {
    // Precalculate vectors and dot products
    vec3  H     = normalize(V + L);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);
    float dotHV = clamp(dot(H, V), 0.0, 1.0);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);

    // Light color fixed
    // vec3 lightColor = vec3(1.0);

    vec3 color = vec3(0.0);

    float rroughness = max(0.05, roughness);
    // D = Normal distribution (Distribution of the microfacets)
    float D = D_GGX(dotNH, rroughness);
    // G = Geometric shadowing term (Microfacets shadowing)
    float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
    // F = Fresnel factor (Reflectance depending on angle of incidence)
    vec3 F = F_Schlick(dotHV, F0);

    vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
    vec3 kD   = (vec3(1.0) - F) * (1.0 - metallic);

    color += (kD * base_color / PI + (1.0 - kD) * spec);
    // color += (kD * basecolor / PI + spec) * dotNL;
    // color += (kD * basecolor / PI + spec) * dotNL * lightColor;

    return color;
}

vec2 ndcxy_to_uv(vec2 ndcxy) { return ndcxy * vec2(0.5, 0.5) + vec2(0.5, 0.5); }

vec2 uv_to_ndcxy(vec2 uv) { return uv * vec2(2.0, 2.0) + vec2(-1.0, -1.0); }