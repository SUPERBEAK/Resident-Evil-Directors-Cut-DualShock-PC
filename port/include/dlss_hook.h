#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

using Microsoft::WRL::ComPtr;

namespace Render {

enum class UpscalerMode {
    Off = 0,             // Native PS1 320x240 integer scaled
    BicubicSharp = 1,    // Hardware spatial bicubic sharpening upscaler
    DLSS_Quality = 2,    // NVIDIA DLSS Quality mode (1.5x scale)
    DLSS_Balanced = 3,   // NVIDIA DLSS Balanced mode (1.7x scale)
    DLSS_Performance = 4,// NVIDIA DLSS Performance mode (2.0x scale)
    DLSS_UltraPerf = 5   // NVIDIA DLSS Ultra Performance mode (3.0x scale)
};

struct DLSSConstants {
    float jitterOffsetX;
    float jitterOffsetY;
    float motionScaleX;
    float motionScaleY;
    uint32_t renderWidth;
    uint32_t renderHeight;
    uint32_t targetWidth;
    uint32_t targetHeight;
    int resetHistory;
};

struct DLSSRenderInputs {
    ID3D12Resource* pColorIn;           // Low-res color buffer (e.g. 320x240)
    ID3D12Resource* pMotionVectors;     // 2D screen UV motion vectors (DXGI_FORMAT_R16G16_FLOAT)
    ID3D12Resource* pDepth;             // Depth buffer
    ID3D12Resource* pOutput;            // High-res resolved destination target (e.g. 4K / 1440p / 1080p)
    float jitterX;                      // Current subpixel jitter offset in pixel units
    float jitterY;
    bool resetHistory;                  // True on camera cuts or room transitions
};

class DLSSHook {
public:
    DLSSHook();
    ~DLSSHook();

    bool Initialize(ID3D12Device* device, uint32_t renderWidth, uint32_t renderHeight, uint32_t targetWidth, uint32_t targetHeight);
    void Shutdown();

    // Check if nvngx_dlss.dll or Streamline is present in the game directory
    bool ProbeDLSSLibraries();

    // Execute upscale pass (DLSS if library present, otherwise bicubic resolve)
    bool Evaluate(ID3D12GraphicsCommandList* cmdList, const DLSSRenderInputs& inputs);

    bool IsDLSSAvailable() const { return m_DLSSAvailable; }
    UpscalerMode GetCurrentMode() const { return m_CurrentMode; }
    void SetMode(UpscalerMode mode) { m_CurrentMode = mode; }
    const wchar_t* GetModeName() const;

    float GetScaleFactor() const;

private:
    bool CreateResolveResources(ID3D12Device* device);
    bool CreateResolvePipeline(ID3D12Device* device);

    ID3D12Device* m_Device = nullptr;
    HMODULE m_hNgxModule = nullptr;
    HMODULE m_hStreamlineModule = nullptr;

    // Dynamic NVSDK NGX API bindings for DLSS / DLSS 5 Neural Rendering
    typedef int (*PFN_NVSDK_NGX_D3D12_Init)(unsigned long long InAppId, const wchar_t* InPath, ID3D12Device* InDev, const void* InFeatureInfo, unsigned int InVersion);
    typedef int (*PFN_NVSDK_NGX_D3D12_GetCapabilityParameters)(void** OutParams);
    typedef int (*PFN_NVSDK_NGX_D3D12_CreateFeature)(ID3D12GraphicsCommandList* InCmdList, int InFeatureID, void* InParams, void** OutHandle);
    typedef int (*PFN_NVSDK_NGX_D3D12_EvaluateFeature)(ID3D12GraphicsCommandList* InCmdList, const void* InFeatureHandle, const void* InParams, void* InCallback);
    typedef int (*PFN_NVSDK_NGX_D3D12_ReleaseFeature)(void* InHandle);
    typedef int (*PFN_NVSDK_NGX_D3D12_Shutdown1)(ID3D12Device* InDev);

    PFN_NVSDK_NGX_D3D12_Init m_pfnInit = nullptr;
    PFN_NVSDK_NGX_D3D12_GetCapabilityParameters m_pfnGetParams = nullptr;
    PFN_NVSDK_NGX_D3D12_CreateFeature m_pfnCreateFeature = nullptr;
    PFN_NVSDK_NGX_D3D12_EvaluateFeature m_pfnEvaluateFeature = nullptr;
    PFN_NVSDK_NGX_D3D12_ReleaseFeature m_pfnReleaseFeature = nullptr;
    PFN_NVSDK_NGX_D3D12_Shutdown1 m_pfnShutdown = nullptr;

    void* m_NgxParameters = nullptr;
    void* m_NgxFeatureHandle = nullptr;
    bool m_NgxInitialized = false;


    bool m_DLSSAvailable = false;
    std::wstring m_DLSSPath;
    std::wstring m_DLSSVersionString;

    UpscalerMode m_CurrentMode = UpscalerMode::BicubicSharp;

    uint32_t m_RenderWidth = 320;
    uint32_t m_RenderHeight = 240;
    uint32_t m_TargetWidth = 1280;
    uint32_t m_TargetHeight = 960;

    // Fallback Bicubic / Bilinear Resolve Pipeline
    ComPtr<ID3D12RootSignature> m_ResolveRootSig;
    ComPtr<ID3D12PipelineState> m_ResolvePSO;
    ComPtr<ID3D12Resource> m_ResolveConstantBuffer;
    uint8_t* m_ResolveCbvData = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_ResolveSrvHeap;
};

} // namespace Render
