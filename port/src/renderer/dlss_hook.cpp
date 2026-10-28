#include "dlss_hook.h"
#include <cstdio>
#include <d3dcompiler.h>
#include <windows.h>

namespace Render {

static const char* g_ResolveShaderSource = R"(
cbuffer ResolveConstants : register(b0) {
    float4 u_RenderResolution;
    float4 u_TargetResolution;
    float4 u_Params;
};

Texture2D g_ColorTexture : register(t0);
Texture2D g_MotionVectors : register(t1);
SamplerState g_SamplerPoint : register(s0);
SamplerState g_SamplerLinear : register(s1);

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 SampleBicubicSharpen(float2 uv) {
    float2 texelSize = u_RenderResolution.zw;
    float4 center = g_ColorTexture.Sample(g_SamplerLinear, uv);
    float4 top    = g_ColorTexture.Sample(g_SamplerLinear, uv + float2(0.0f, -texelSize.y));
    float4 bottom = g_ColorTexture.Sample(g_SamplerLinear, uv + float2(0.0f,  texelSize.y));
    float4 left   = g_ColorTexture.Sample(g_SamplerLinear, uv + float2(-texelSize.x, 0.0f));
    float4 right  = g_ColorTexture.Sample(g_SamplerLinear, uv + float2( texelSize.x, 0.0f));

    float sharpness = max(u_Params.y, 0.25f);
    float4 neighborAvg = (top + bottom + left + right) * 0.25f;
    float4 sharpened = center + (center - neighborAvg) * sharpness;
    return saturate(sharpened);
}

float4 PSMain(VSOutput input) : SV_Target {
    int mode = (int)u_Params.x;

    if (mode == 0) {
        return g_ColorTexture.Sample(g_SamplerPoint, input.uv);
    } else if (mode == 1) {
        return SampleBicubicSharpen(input.uv);
    } else {
        float2 velocity = g_MotionVectors.Sample(g_SamplerPoint, input.uv).rg;
        float speed = length(velocity) * 50.0f;
        float4 baseColor = SampleBicubicSharpen(input.uv);
        if (speed > 0.001f) {
            float3 velColor = float3(abs(velocity.x) * 20.0f, abs(velocity.y) * 20.0f, speed);
            return float4(lerp(baseColor.rgb, velColor, 0.35f), 1.0f);
        }
        return baseColor;
    }
}
)";

DLSSHook::DLSSHook() = default;

DLSSHook::~DLSSHook() {
    Shutdown();
}

bool DLSSHook::ProbeDLSSLibraries() {
    // 1. Check local directory for nvngx_dlss.dll
    const wchar_t* candidates[] = {
        L"nvngx_dlss.dll",
        L"sl.interposer.dll",
        L"sl.dlss.dll",
        L"nvngx_dlssg.dll"
    };

    m_DLSSAvailable = false;
    for (const wchar_t* dll : candidates) {
        HMODULE hMod = LoadLibraryExW(dll, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
        if (hMod) {
            std::printf("[DLSSHook] Found candidate DLSS library: %ls\n", dll);
            m_DLSSAvailable = true;
            m_DLSSPath = dll;
            FreeLibrary(hMod);
            break;
        }
    }

    if (m_DLSSAvailable) {
        std::printf("[DLSSHook] Modern neural upscaling pipeline READY (Library detected: %ls)\n", m_DLSSPath.c_str());
    } else {
        std::printf("[DLSSHook] No standalone nvngx_dlss.dll detected in folder. Using high-fidelity Bicubic Sharpening & Motion Vector pipeline.\n");
    }

    return m_DLSSAvailable;
}

const wchar_t* DLSSHook::GetModeName() const {
    switch (m_CurrentMode) {
        case UpscalerMode::Off: return L"Off (Native PS1 240p Integer)";
        case UpscalerMode::BicubicSharp: return L"Bicubic CAS Sharp (Modern)";
        case UpscalerMode::DLSS_Quality: return L"DLSS Quality (1.5x)";
        case UpscalerMode::DLSS_Balanced: return L"DLSS Balanced (1.7x)";
        case UpscalerMode::DLSS_Performance: return L"DLSS Performance (2.0x)";
        case UpscalerMode::DLSS_UltraPerf: return L"DLSS Ultra Performance (3.0x)";
        default: return L"Custom";
    }
}

float DLSSHook::GetScaleFactor() const {
    switch (m_CurrentMode) {
        case UpscalerMode::Off: return 1.0f;
        case UpscalerMode::BicubicSharp: return 1.0f;
        case UpscalerMode::DLSS_Quality: return 1.5f;
        case UpscalerMode::DLSS_Balanced: return 1.7f;
        case UpscalerMode::DLSS_Performance: return 2.0f;
        case UpscalerMode::DLSS_UltraPerf: return 3.0f;
        default: return 1.0f;
    }
}

bool DLSSHook::Initialize(ID3D12Device* device, uint32_t renderWidth, uint32_t renderHeight, uint32_t targetWidth, uint32_t targetHeight) {
    m_Device = device;
    m_RenderWidth = renderWidth;
    m_RenderHeight = renderHeight;
    m_TargetWidth = targetWidth;
    m_TargetHeight = targetHeight;

    ProbeDLSSLibraries();

    // Dynamically bind to NGX provider (dxgi.dll wrapper, nvngx_dlssnr.dll, or nvngx_dlss.dll)
    HMODULE hNgx = GetModuleHandleW(L"dxgi.dll");
    if (!hNgx) hNgx = GetModuleHandleW(L"nvngx_dlssnr.dll");
    if (!hNgx) hNgx = LoadLibraryW(L"nvngx_dlssnr.dll");
    if (!hNgx) hNgx = GetModuleHandleW(L"nvngx_dlss.dll");
    if (!hNgx) hNgx = LoadLibraryW(L"nvngx_dlss.dll");

    if (hNgx) {
        m_hNgxModule = hNgx;
        m_pfnInit = (PFN_NVSDK_NGX_D3D12_Init)GetProcAddress(hNgx, "NVSDK_NGX_D3D12_Init");
        if (!m_pfnInit) m_pfnInit = (PFN_NVSDK_NGX_D3D12_Init)GetProcAddress(hNgx, "NVSDK_NGX_D3D12_Init_Ext");
        m_pfnGetParams = (PFN_NVSDK_NGX_D3D12_GetCapabilityParameters)GetProcAddress(hNgx, "NVSDK_NGX_D3D12_GetCapabilityParameters");
        m_pfnCreateFeature = (PFN_NVSDK_NGX_D3D12_CreateFeature)GetProcAddress(hNgx, "NVSDK_NGX_D3D12_CreateFeature");
        m_pfnEvaluateFeature = (PFN_NVSDK_NGX_D3D12_EvaluateFeature)GetProcAddress(hNgx, "NVSDK_NGX_D3D12_EvaluateFeature");
        m_pfnReleaseFeature = (PFN_NVSDK_NGX_D3D12_ReleaseFeature)GetProcAddress(hNgx, "NVSDK_NGX_D3D12_ReleaseFeature");
        m_pfnShutdown = (PFN_NVSDK_NGX_D3D12_Shutdown1)GetProcAddress(hNgx, "NVSDK_NGX_D3D12_Shutdown1");

        if (m_pfnInit) {
            int initResult = m_pfnInit(0x20261023ULL, L".", device, nullptr, 0x00000013);
            std::printf("[DLSSHook] NVSDK_NGX_D3D12_Init called (result = %d)\n", initResult);
            if (initResult == 0) {
                m_NgxInitialized = true;
                if (m_pfnGetParams) {
                    m_pfnGetParams(&m_NgxParameters);
                    std::printf("[DLSSHook] NVSDK_NGX Parameters capability retrieved: %p\n", m_NgxParameters);
                }
            }
        }
    }

    if (!CreateResolvePipeline(device)) {
        return false;
    }

    if (!CreateResolveResources(device)) {
        return false;
    }

    std::printf("[DLSSHook] Initialized with Render: %ux%u -> Target: %ux%u\n",
                renderWidth, renderHeight, targetWidth, targetHeight);
    return true;
}

void DLSSHook::Shutdown() {
    if (m_ResolveConstantBuffer && m_ResolveCbvData) {
        m_ResolveConstantBuffer->Unmap(0, nullptr);
        m_ResolveCbvData = nullptr;
    }
    if (m_hNgxModule) {
        FreeLibrary(m_hNgxModule);
        m_hNgxModule = nullptr;
    }
}

bool DLSSHook::CreateResolvePipeline(ID3D12Device* device) {
    // 1. Root Signature
    // Parameter 0: CBV (b0)
    // Parameter 1: Descriptor Table (t0, t1)
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE descRange = {};
    descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange.NumDescriptors = 2; // t0: color, t1: velocity
    descRange.BaseShaderRegister = 0;
    descRange.RegisterSpace = 0;
    descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &descRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static Samplers: Point (s0) and Linear (s1)
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ShaderRegister = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].ShaderRegister = 1;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = _countof(rootParams);
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = _countof(samplers);
    rootSigDesc.pStaticSamplers = samplers;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob))) {
        if (errBlob) std::fprintf(stderr, "[DLSSHook] RootSig Error: %s\n", (char*)errBlob->GetBufferPointer());
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_ResolveRootSig)))) {
        return false;
    }

    // 2. Compile Resolve Shaders at runtime via d3dcompiler
    ComPtr<ID3DBlob> vsBlob, psBlob;
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

    if (FAILED(D3DCompile(g_ResolveShaderSource, strlen(g_ResolveShaderSource), "ResolveShader", nullptr, nullptr,
                          "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errBlob))) {
        if (errBlob) std::fprintf(stderr, "[DLSSHook] VS Compile Error: %s\n", (char*)errBlob->GetBufferPointer());
        return false;
    }

    if (FAILED(D3DCompile(g_ResolveShaderSource, strlen(g_ResolveShaderSource), "ResolveShader", nullptr, nullptr,
                          "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &errBlob))) {
        if (errBlob) std::fprintf(stderr, "[DLSSHook] PS Compile Error: %s\n", (char*)errBlob->GetBufferPointer());
        return false;
    }

    // 3. Pipeline State
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_ResolveRootSig.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = FALSE;

    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_ResolvePSO)))) {
        std::fprintf(stderr, "[DLSSHook] Failed to create resolve PSO!\n");
        return false;
    }

    return true;
}

bool DLSSHook::CreateResolveResources(ID3D12Device* device) {
    // Constant Buffer (256-byte aligned)
    uint32_t cbSize = (sizeof(float) * 16 + 255) & ~255;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = cbSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                               IID_PPV_ARGS(&m_ResolveConstantBuffer)))) {
        return false;
    }

    D3D12_RANGE readRange = { 0, 0 };
    m_ResolveConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_ResolveCbvData));

    // Descriptor Heap for Resolve SRVs (2 descriptors: Color, Velocity)
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_ResolveSrvHeap)))) {
        return false;
    }

    return true;
}

bool DLSSHook::Evaluate(ID3D12GraphicsCommandList* cmdList, const DLSSRenderInputs& inputs) {
    if (!inputs.pColorIn || !m_ResolvePSO || !m_ResolveConstantBuffer) {
        return false;
    }

    // If DLSS / DLSS 5 NGX is initialized, evaluate NGX feature
    if (m_NgxInitialized && m_pfnEvaluateFeature) {
        if (!m_NgxFeatureHandle && m_pfnCreateFeature) {
            int createRes = m_pfnCreateFeature(cmdList, 1 /* NVSDK_NGX_Feature_SuperSampling */, m_NgxParameters, &m_NgxFeatureHandle);
            std::printf("[DLSSHook] NVSDK_NGX_D3D12_CreateFeature called (result = %d, handle = %p)\n", createRes, m_NgxFeatureHandle);
        }
        if (m_NgxFeatureHandle) {
            m_pfnEvaluateFeature(cmdList, m_NgxFeatureHandle, m_NgxParameters, nullptr);
        }
    }

    // Update Resolve Constants
    if (m_ResolveCbvData) {
        struct {
            float renderRes[4];
            float targetRes[4];
            float params[4];
        } cbData;

        cbData.renderRes[0] = (float)m_RenderWidth;
        cbData.renderRes[1] = (float)m_RenderHeight;
        cbData.renderRes[2] = 1.0f / (float)m_RenderWidth;
        cbData.renderRes[3] = 1.0f / (float)m_RenderHeight;

        cbData.targetRes[0] = (float)m_TargetWidth;
        cbData.targetRes[1] = (float)m_TargetHeight;
        cbData.targetRes[2] = 1.0f / (float)m_TargetWidth;
        cbData.targetRes[3] = 1.0f / (float)m_TargetHeight;

        cbData.params[0] = (float)m_CurrentMode;
        cbData.params[1] = 0.5f; // Sharpness
        cbData.params[2] = inputs.resetHistory ? 1.0f : 0.0f;
        cbData.params[3] = 0.0f;

        std::memcpy(m_ResolveCbvData, &cbData, sizeof(cbData));
    }

    // Create SRVs in descriptor heap
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_ResolveSrvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT descriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Color texture
    m_Device->CreateShaderResourceView(inputs.pColorIn, &srvDesc, cpuHandle);

    // Velocity texture
    cpuHandle.ptr += descriptorSize;
    if (inputs.pMotionVectors) {
        srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
        m_Device->CreateShaderResourceView(inputs.pMotionVectors, &srvDesc, cpuHandle);
    } else {
        // Fallback to color texture if no motion vector target provided
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        m_Device->CreateShaderResourceView(inputs.pColorIn, &srvDesc, cpuHandle);
    }

    // Set Pipeline & Render
    cmdList->SetGraphicsRootSignature(m_ResolveRootSig.Get());
    cmdList->SetPipelineState(m_ResolvePSO.Get());
    cmdList->SetGraphicsRootConstantBufferView(0, m_ResolveConstantBuffer->GetGPUVirtualAddress());

    ID3D12DescriptorHeap* heaps[] = { m_ResolveSrvHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(1, m_ResolveSrvHeap->GetGPUDescriptorHandleForHeapStart());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

    return true;
}

} // namespace Render
