struct SceneParams
{
    float4x4 viewProj;
    float4x4 model;
    float3   cameraPos;
    float    time;
};
ConstantBuffer<SceneParams> gScene;

SamplerState gSampler;

float sn_hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    float d = dot(p, p + float2(45.32, 45.32));
    p += float2(d, d);
    return frac(p.x * p.y);
}

float sn_valueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = sn_hash21(i);
    float b = sn_hash21(i + float2(1.0, 0.0));
    float c = sn_hash21(i + float2(0.0, 1.0));
    float d = sn_hash21(i + float2(1.0, 1.0));
    float2 u = f * f * (float2(3.0, 3.0) - 2.0 * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float sn_fbm(float2 p, int octaves)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; i++)
    {
        value += amplitude * sn_valueNoise(p);
        p = p * 2.0 + float2(17.0, 17.0);
        amplitude *= 0.5;
    }
    return value;
}

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD1;
    float3 normal   : TEXCOORD2;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    float4 world = mul(gScene.model, float4(input.position, 1.0));
    output.worldPos = world.xyz;
    output.normal = mul((float3x3)gScene.model, input.normal);
    output.uv = input.uv;
    output.position = mul(gScene.viewProj, world);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    // Color (#7)
    float3 n7_RGB = float4(0.04, 0.07, 0.28, 1.0).rgb;
    // Color (#8)
    float3 n8_RGB = float4(1.0, 0.45, 0.1, 1.0).rgb;
    // UV (#1)
    float2 n1_UV = input.uv;
    // Time (#2)
    float n2_Time = gScene.time;
    // Multiply (#3)
    float n3_Out = n2_Time * 0.08;
    // Combine (#4)
    float2 n4_RG = float2(n3_Out, 0.0);
    // Tiling And Offset (#5)
    float2 n5_Out = n1_UV * float2(6.0, 6.0) + n4_RG;
    // Fractal Noise (#6)
    float n6_Out = sn_fbm(n5_Out * 1.0, 5);
    // Lerp (#9)
    float3 n9_Out = lerp(n7_RGB, n8_RGB, float3(n6_Out));
    // Fresnel (#10)
    float n10_Out = pow(1.0 - saturate(dot(normalize(input.normal), normalize(gScene.cameraPos - input.worldPos))), 3.0);
    // Multiply (#11)
    float3 n11_Out = float3(n10_Out) * n8_RGB;
    // Fragment Output (#12)
    return float4(n9_Out + n11_Out, 1.0);
}
