// Preview stand-in for the engine's kes_common.slang.
//
// The preview compiles the exact shader that gets exported, with this file on
// the include path instead of the engine's. It keeps the API a generated shader
// uses and the same BRDF, material logic and flags, but replaces shadows, fog
// and the sky cube with two fixed lights and an analytic sky, so the result
// compiles to GLSL that plain OpenGL accepts: combined samplers, set 0 only.
//
// Keep KesSurface, KesMaterialSurface and the BRDF in sync with
// engine/shaders/kes_common.slang.

#pragma once

static const float kPi = 3.14159265;

[[vk::binding(0, 0)]]
cbuffer SceneCB : register(b0)
{
    row_major float4x4 uViewProj;
    float4 uCameraPosition;
    float4 uEnvironmentParams;   // y = intensity, z = time in seconds
};

[[vk::binding(1, 0)]]
cbuffer ModelCB : register(b1)
{
    row_major float4x4 uWorldViewProj;
    row_major float4x4 uWorld;
};

[[vk::binding(2, 0)]]
cbuffer MaterialCB : register(b2)
{
    float4 uBaseColor;
    float4 uEmissive;          // rgb = emissive, a = alpha cutoff
    float4 uPbr;               // x = metallic, y = roughness, z = normal scale, w = occlusion strength
    uint4 uMaterialFlags;
};

static const uint kMaterialFlagLit = 1u;
static const uint kMaterialFlagDoubleSided = 4u;
static const uint kMaterialFlagHasBase = 8u;
static const uint kMaterialFlagHasNormal = 16u;
static const uint kMaterialFlagHasMetallicRoughness = 32u;
static const uint kMaterialFlagHasOcclusion = 64u;
static const uint kMaterialFlagHasEmissive = 128u;

float KesTime()
{
    return uEnvironmentParams.z;
}

bool KesMaterialHas(uint flag)
{
    return (uMaterialFlags.x & flag) != 0u;
}

[[vk::binding(3, 0)]] Sampler2D uBaseTexture : register(t0);
[[vk::binding(4, 0)]] Sampler2D uNormalTexture : register(t1);
[[vk::binding(5, 0)]] Sampler2D uMetallicRoughnessTexture : register(t2);
[[vk::binding(6, 0)]] Sampler2D uOcclusionTexture : register(t3);
[[vk::binding(7, 0)]] Sampler2D uEmissiveTexture : register(t4);

struct MeshVertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
    [[vk::location(3)]] float4 color    : COLOR;
};

struct MeshVertexResult
{
    float4 clipPosition;
    float3 worldPosition;
    float3 worldNormal;
    float4 color;
    float2 uv;
};

MeshVertexResult KesMeshVertex(MeshVertexInput input)
{
    float4 localPos = float4(input.position, 1.0);
    MeshVertexResult result;
    result.worldPosition = mul(localPos, uWorld).xyz;
    result.worldNormal = mul(float4(input.normal, 0.0), uWorld).xyz;
    result.clipPosition = mul(localPos, uWorldViewProj);
    result.color = input.color;
    result.uv = input.uv;
    return result;
}

float KesDistributionGGX(float NoH, float a)
{
    float a2 = a * a;
    float d = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (kPi * d * d + 1e-7);
}

float KesVisibilitySmithGGX(float NoV, float NoL, float a)
{
    float a2 = a * a;
    float v = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float l = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (v + l + 1e-7);
}

float3 KesFresnelSchlick(float3 f0, float VoH)
{
    float f = pow(1.0 - VoH, 5.0);
    return f0 + (1.0 - f0) * f;
}

float3 KesEnvironmentBRDF(float3 f0, float roughness, float NoV)
{
    const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}

float3 KesPerturbNormal(float3 n, float3 worldPos, float2 uv, float3 tangentNormal)
{
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    float3 dp2perp = cross(dp2, n);
    float3 dp1perp = cross(n, dp1);
    float3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float invMax = rsqrt(max(max(dot(t, t), dot(b, b)), 1e-12));
    return normalize(t * (tangentNormal.x * invMax) + b * (tangentNormal.y * invMax) + n * tangentNormal.z);
}

// Matches the gradient drawn behind the mesh (PreviewRenderer.cpp).
float3 KesPreviewSky(float3 d, float blur)
{
    float3 zenith = float3(0.25, 0.42, 0.75);
    float3 horizon = float3(0.75, 0.80, 0.85);
    float3 ground = float3(0.18, 0.16, 0.14);
    float width = lerp(0.02, 1.0, blur);
    float3 sky = lerp(horizon, zenith, saturate(d.y));
    return lerp(ground, sky, smoothstep(-width, width, d.y));
}

struct SurfaceInput
{
    float3 worldPosition;
    float3 worldNormal;
    float4 vertexColor;
    float2 uv;
    bool frontFace;
};

struct KesSurface
{
    float3 baseColor;
    float alpha;
    float3 emissive;
    float3 normal;
    float metallic;
    float roughness;
    float occlusion;
};

float3 KesGeometricNormal(SurfaceInput s)
{
    float3 n = normalize(s.worldNormal);
    return !s.frontFace && KesMaterialHas(kMaterialFlagDoubleSided) ? -n : n;
}

float3 KesTangentNormal(SurfaceInput s, float3 tangentNormal)
{
    return KesPerturbNormal(KesGeometricNormal(s), s.worldPosition, s.uv, normalize(tangentNormal));
}

KesSurface KesMaterialSurface(SurfaceInput s)
{
    float4 baseColor = uBaseColor * s.vertexColor;
    if (KesMaterialHas(kMaterialFlagHasBase))
        baseColor *= uBaseTexture.Sample(s.uv);

    KesSurface m;
    m.baseColor = baseColor.rgb;
    m.alpha = baseColor.a;
    m.emissive = uEmissive.rgb;
    if (KesMaterialHas(kMaterialFlagHasEmissive))
        m.emissive *= uEmissiveTexture.Sample(s.uv).rgb;
    m.normal = KesGeometricNormal(s);
    m.metallic = uPbr.x;
    m.roughness = uPbr.y;
    m.occlusion = 1.0;

    if (KesMaterialHas(kMaterialFlagLit))
    {
        if (KesMaterialHas(kMaterialFlagHasNormal))
        {
            float3 tangentNormal = uNormalTexture.Sample(s.uv).xyz * 2.0 - 1.0;
            tangentNormal.xy *= uPbr.z;
            m.normal = KesTangentNormal(s, tangentNormal);
        }
        if (KesMaterialHas(kMaterialFlagHasMetallicRoughness))
        {
            float4 mr = uMetallicRoughnessTexture.Sample(s.uv);
            m.roughness *= mr.g;
            m.metallic *= mr.b;
        }
        if (KesMaterialHas(kMaterialFlagHasOcclusion))
            m.occlusion = lerp(1.0, uOcclusionTexture.Sample(s.uv).r, uPbr.w);
    }
    return m;
}

float3 KesPreviewLight(float3 n, float3 v, float3 l, float3 radiance, float3 f0, float3 diffuseColor, float a, float NoV)
{
    float NoL = dot(n, l);
    if (NoL <= 0.0)
        return float3(0.0, 0.0, 0.0);
    float3 h = normalize(v + l);
    float NoH = saturate(dot(n, h));
    float VoH = saturate(dot(v, h));
    float3 f = KesFresnelSchlick(f0, VoH);
    float3 specular = KesDistributionGGX(NoH, a) * KesVisibilitySmithGGX(NoV, NoL, a) * f;
    float3 diffuse = (1.0 - f) * diffuseColor / kPi;
    return (diffuse + specular) * radiance * NoL;
}

float4 KesShadeMaterial(SurfaceInput s, KesSurface m)
{
    if (uEmissive.a > 0.0 && m.alpha < uEmissive.a)
        discard;

    if (!KesMaterialHas(kMaterialFlagLit))
        return float4(m.baseColor + m.emissive, m.alpha);

    float3 n = m.normal;
    float metallic = saturate(m.metallic);
    float roughness = clamp(m.roughness, 0.03, 1.0);

    float3 v = normalize(uCameraPosition.xyz - s.worldPosition);
    float NoV = max(dot(n, v), 1e-4);
    float a = roughness * roughness;
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), m.baseColor, metallic);
    float3 diffuseColor = m.baseColor * (1.0 - metallic);

    float3 direct = KesPreviewLight(n, v, normalize(float3(0.5, 0.8, 0.45)), float3(3.0, 2.9, 2.75), f0, diffuseColor, a, NoV);
    direct += KesPreviewLight(n, v, normalize(float3(-0.7, 0.15, -0.5)), float3(0.5, 0.55, 0.65), f0, diffuseColor, a, NoV);

    float3 r = reflect(-v, n);
    float3 ambient = (diffuseColor * KesPreviewSky(n, 1.0) + KesPreviewSky(r, roughness) * KesEnvironmentBRDF(f0, roughness, NoV)) *
                     (m.occlusion * uEnvironmentParams.y);

    return float4(direct + ambient + m.emissive, m.alpha);
}

float4 KesShadeSurface(SurfaceInput s)
{
    return KesShadeMaterial(s, KesMaterialSurface(s));
}
