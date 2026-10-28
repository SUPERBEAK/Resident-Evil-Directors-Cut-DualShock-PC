#pragma once
#include "background_loader.h"
#include "dlss_hook.h"
#include "halton_jitter.h"
#include "ps1_pipeline.h"
#include "psx_tmd.h"
#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Render {

class DX12Renderer {
public:
  static constexpr uint32_t FRAME_COUNT = 2;
  static constexpr uint32_t PS1_NATIVE_WIDTH = 320;
  static constexpr uint32_t PS1_NATIVE_HEIGHT = 240;

  DX12Renderer();
  ~DX12Renderer();

  bool Initialize(HWND hWnd, uint32_t width, uint32_t height);
  void Shutdown();

  void BeginFrame();
  void EndFrame();
  void Resize(uint32_t width, uint32_t height);

  void RenderBackground();
  void RenderTestTriangle(float angle);

  bool LoadRoomBackground(const std::wstring &bssFilePath, uint32_t cutIndex);
  bool LoadTmdModel(const std::wstring &tmdFilePath);
  void RenderTmdMesh(float angleX, float angleY, float scale);

  void SetUpscalerMode(UpscalerMode mode) { m_DlssHook.SetMode(mode); }
  UpscalerMode GetUpscalerMode() const { return m_DlssHook.GetCurrentMode(); }
  const wchar_t *GetUpscalerModeName() const {
    return m_DlssHook.GetModeName();
  }
  bool IsDLSSAvailable() const { return m_DlssHook.IsDLSSAvailable(); }

  ID3D12Device *GetDevice() const { return m_Device.Get(); }
  ID3D12CommandQueue *GetCommandQueue() const { return m_CommandQueue.Get(); }
  ID3D12GraphicsCommandList *GetCommandList() const {
    return m_CommandList.Get();
  }
  PS1Pipeline &GetPS1Pipeline() { return m_Pipeline; }
  BackgroundLoader &GetBackgroundLoader() { return m_BgLoader; }

private:
  bool CreateDeviceAndQueue();
  bool CreateSwapChain(HWND hWnd);
  bool CreateRenderTargets();
  bool CreateCommandObjects();
  bool CreateSyncObjects();
  bool CreateTestGeometry();
  bool CreateOffscreenRenderTargets();
  void WaitForGpu();

  HWND m_Hwnd = nullptr;
  uint32_t m_Width = 1280;
  uint32_t m_Height = 960;
  uint32_t m_FrameIndex = 0;

  ComPtr<IDXGIFactory6> m_Factory;
  ComPtr<ID3D12Device> m_Device;
  ComPtr<ID3D12CommandQueue> m_CommandQueue;
  ComPtr<IDXGISwapChain3> m_SwapChain;
  ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
  UINT m_RtvDescriptorSize = 0;
  ComPtr<ID3D12Resource> m_RenderTargets[FRAME_COUNT];
  ComPtr<ID3D12Resource> m_DepthStencil;
  ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
  ComPtr<ID3D12CommandAllocator> m_CommandAllocators[FRAME_COUNT];
  ComPtr<ID3D12GraphicsCommandList> m_CommandList;

  // Synchronization
  HANDLE m_FenceEvent = nullptr;
  ComPtr<ID3D12Fence> m_Fence;
  uint64_t m_FenceValues[FRAME_COUNT] = {0, 0};

  D3D12_VIEWPORT m_Viewport{};
  D3D12_RECT m_ScissorRect{};

  // PS1 Authentic Pipeline
  PS1Pipeline m_Pipeline;
  ComPtr<ID3D12Resource> m_VertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView{};
  ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
  ComPtr<ID3D12Resource> m_DummyTexture;

  // Background Loader
  BackgroundLoader m_BgLoader;

  // TMD Model
  ComPtr<ID3D12Resource> m_TmdVertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_TmdVertexBufferView{};
  uint32_t m_TmdVertexCount = 0;

  // Offscreen Render Targets (320x240 native PS1 with Motion Vectors)
  ComPtr<ID3D12Resource> m_OffscreenColor;
  ComPtr<ID3D12Resource> m_OffscreenVelocity;
  ComPtr<ID3D12DescriptorHeap> m_OffscreenRtvHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_OffscreenColorRtv{};
  D3D12_CPU_DESCRIPTOR_HANDLE m_OffscreenVelocityRtv{};

  D3D12_VIEWPORT m_NativeViewport{};
  D3D12_RECT m_NativeScissorRect{};

  // Upscaler & DLSS Hook
  DLSSHook m_DlssHook;
  uint32_t m_TotalFrames = 0;
  XMMATRIX m_PrevWorldViewProj = XMMatrixIdentity();
};

} // namespace Render
