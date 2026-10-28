// Resident Evil Director's Cut: Dual Shock (PC Port)
// Upscale & Resolve Pass Shader (HLSL 5.1 / 6.0)

cbuffer ResolveConstants : register(b0) {
    float4 u_RenderResolution; // xy: render res (e.g. 320, 240), zw: 1.0 / render res
    float4 u_TargetResolution; // xy: target res (e.g. 1920, 1080), zw: 1.0 / target res
    float4 u_Params;           // x: mode (0=PS1, 1=BicubicSharp, 2=DLSS/MotionDebug), y: sharpness, z: resetHistory
};

Texture2D g_ColorTexture : register(t0);
Texture2D g_MotionVectors : register(t1);
SamplerState g_SamplerPoint : register(s0);
SamplerState g_SamplerLinear : register(s1);

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// Fullscreen procedural quad (1 big triangle covering -1..1 NDC)
VSOutput VSMain(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

// 5-tap Catmull-Rom / CAS style sharpening filter
float4 SampleBicubicSharpen(float2 uv) {
    float2 texelSize = u_RenderResolution.zw;
    float4 center = g_ColorTexture.Sample(g_SamplerLinear, uv);
    float4 top    = g_ColorTexture.Sample(g_SamplerLinear, uv + float2(0.0f, -texelSize.y));
    float4 bottom = g_ColorTexture.Sample(g_SamplerLinear, uv + float2(0.0f,  texelSize.y));
    float4 left   = g_ColorTexture.Sample(g_SamplerLinear, uv + float2(-texelSize.x, 0.0f));
    float4 right  = g_ColorTexture.Sample(g_SamplerLinear, uv + float2( texelSize.x, 0.0f));

    // Contrast Adaptive Sharpening weight
    float sharpness = max(u_Params.y, 0.25f);
    float4 neighborAvg = (top + bottom + left + right) * 0.25f;
    float4 sharpened = center + (center - neighborAvg) * sharpness;
    return saturate(sharpened);
}

float4 PSMain(VSOutput input) : SV_Target {
    int mode = (int)u_Params.x;

    if (mode == 0) {
        // Mode 0: Authentic PS1 Nearest-Neighbor pixel art display
        return g_ColorTexture.Sample(g_SamplerPoint, input.uv);
    } else if (mode == 1) {
        // Mode 1: High-quality Bicubic Sharpening upscaler (4K / 1440p clean upscale)
        return SampleBicubicSharpen(input.uv);
    } else {
        // Mode 2+: DLSS / Motion Vector Debug visualization
        float2 velocity = g_MotionVectors.Sample(g_SamplerPoint, input.uv).rg;
        float speed = length(velocity) * 50.0f;
        float4 baseColor = SampleBicubicSharpen(input.uv);
        // Subtle velocity visualization tint if motion detected
        if (speed > 0.001f) {
            float3 velColor = float3(abs(velocity.x) * 20.0f, abs(velocity.y) * 20.0f, speed);
            return float4(lerp(baseColor.rgb, velColor, 0.35f), 1.0f);
        }
        return baseColor;
    }
}
