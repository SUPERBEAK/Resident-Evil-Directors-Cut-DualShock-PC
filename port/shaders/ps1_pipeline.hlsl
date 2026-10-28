// Resident Evil Director's Cut: Dual Shock (PC Port)
// Authentic PS1 GPU Pipeline Emulation Shader (HLSL 5.1 / 6.0)

cbuffer SceneConstants : register(b0) {
    row_major float4x4 u_WorldViewProj;
    row_major float4x4 u_PrevWorldViewProj;
    float4 u_ScreenResolution; // xy = target res (e.g. 320, 240), zw = 1.0 / res
    float4 u_RenderFlags;      // x: snapEnabled, y: affineEnabled, z: ditherEnabled, w: color15BitEnabled
    float4 u_BlendMode;        // x: semiTransMode (0=0.5B+0.5F, 1=1.0B+1.0F, 2=1.0B-1.0F, 3=1.0B+0.25F), y: isSemiTrans
    float4 u_JitterOffset;     // xy: current subpixel jitter, zw: previous jitter
};

Texture2D g_Texture : register(t0);
Texture2D g_Clut : register(t1); // Palette texture for 4-bit / 8-bit CLUT
SamplerState g_SamplerPoint : register(s0);

struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD0;
    float3 affineUv : TEXCOORD1; // xy = uv * w, z = w (for affine mapping)
};

// 4x4 PS1 Bayer Dither Matrix (values 0..3 scaled by 1/255.0)
static const float ditherMatrix[4][4] = {
    { -4.0 / 255.0,  0.0 / 255.0, -3.0 / 255.0,  1.0 / 255.0 },
    {  2.0 / 255.0, -2.0 / 255.0,  3.0 / 255.0, -1.0 / 255.0 },
    { -3.0 / 255.0,  1.0 / 255.0, -4.0 / 255.0,  0.0 / 255.0 },
    {  3.0 / 255.0, -1.0 / 255.0,  2.0 / 255.0, -2.0 / 255.0 }
};

PSInput VSMain(VSInput input) {
    PSInput output;

    float4 clipPos = mul(float4(input.position, 1.0f), u_WorldViewProj);

    // 1. Authentic PS1 Vertex Snapping (Fixed-Point Integer Coordinate Truncation)
    if (u_RenderFlags.x > 0.5f && clipPos.w > 0.0001f) {
        float2 ndc = clipPos.xy / clipPos.w;
        // Map NDC (-1..1) to PS1 native framebuffer grid (320x240 or 640x480)
        float2 screenPos = (ndc * 0.5f + 0.5f) * u_ScreenResolution.xy;
        // Truncate/snap to nearest integer texel
        screenPos = floor(screenPos + 0.5f);
        // Map back to NDC
        ndc = (screenPos / u_ScreenResolution.xy - 0.5f) * 2.0f;
        clipPos.xy = ndc * clipPos.w;
    }

    output.position = clipPos;
    output.color = input.color;
    output.uv = input.uv;

    // For affine mapping: multiply UV by W in vertex shader, or keep linear
    if (u_RenderFlags.y > 0.5f) {
        // Affine texture mapping: bypass perspective correction
        output.affineUv = float3(input.uv, 1.0f);
    } else {
        // Perspective-correct mode (modern enhancement)
        output.affineUv = float3(input.uv * clipPos.w, clipPos.w);
    }

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float2 uv;
    if (u_RenderFlags.y > 0.5f) {
        uv = input.affineUv.xy; // Non-perspective affine
    } else {
        uv = input.affineUv.xy / max(input.affineUv.z, 0.0001f);
    }

    float4 texColor = g_Texture.Sample(g_SamplerPoint, uv);
    float4 finalColor = texColor * input.color;

    // Mask transparent pixels (pure black 0,0,0 with STP=0 is transparent in PS1 TIM)
    if (texColor.a < 0.05f) {
        discard;
    }

    // 2. Authentic 4x4 Ordered Bayer Dithering
    if (u_RenderFlags.z > 0.5f) {
        int2 pixelPos = int2(input.position.xy) % 4;
        float dither = ditherMatrix[pixelPos.y][pixelPos.x];
        finalColor.rgb += dither;
    }

    // 3. Authentic 15-bit Color Quantization (5 bits per RGB channel: 32 levels)
    if (u_RenderFlags.w > 0.5f) {
        finalColor.rgb = floor(saturate(finalColor.rgb) * 31.0f + 0.5f) / 31.0f;
    }

    return finalColor;
}
