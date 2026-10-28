#include "dx12_renderer.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace Render {

DX12Renderer::DX12Renderer() {}

DX12Renderer::~DX12Renderer() { Shutdown(); }

bool DX12Renderer::Initialize(HWND hWnd, uint32_t width, uint32_t height) {
  m_Hwnd = hWnd;
  m_Width = width;
  m_Height = height;

  m_Viewport = {
      0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height),
      0.0f, 1.0f};
  m_ScissorRect = {0, 0, static_cast<LONG>(m_Width),
                   static_cast<LONG>(m_Height)};

  if (!CreateDeviceAndQueue())
    return false;
  if (!CreateSwapChain(hWnd))
    return false;
  if (!CreateRenderTargets())
    return false;
  if (!CreateCommandObjects())
    return false;
  if (!CreateSyncObjects())
    return false;

  // Initialize PS1 Authentic HLSL Pipeline
  if (!m_Pipeline.Initialize(m_Device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM)) {
    return false;
  }

  if (!CreateTestGeometry())
    return false;

  std::printf("[DX12] Successfully initialized DirectX 12 renderer & PS1 "
              "pipeline (%ux%u)\n",
              m_Width, m_Height);
  return true;
}

bool DX12Renderer::CreateDeviceAndQueue() {
  UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    std::printf("[DX12] Debug layer enabled\n");
  }
#endif

  if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory)))) {
    std::fprintf(stderr, "[DX12] Failed to create DXGI factory\n");
    return false;
  }

  ComPtr<IDXGIAdapter1> hardwareAdapter;
  for (UINT adapterIndex = 0; SUCCEEDED(m_Factory->EnumAdapterByGpuPreference(
           adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
           IID_PPV_ARGS(&hardwareAdapter)));
       ++adapterIndex) {
    DXGI_ADAPTER_DESC1 desc;
    hardwareAdapter->GetDesc1(&desc);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
      continue;

    if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(),
                                    D3D_FEATURE_LEVEL_12_0,
                                    IID_PPV_ARGS(&m_Device)))) {
      std::wprintf(L"[DX12] Hardware Adapter: %s\n", desc.Description);
      break;
    }
  }

  if (!m_Device) {
    std::fprintf(stderr, "[DX12] Failed to create D3D12 device\n");
    return false;
  }

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (FAILED(m_Device->CreateCommandQueue(&queueDesc,
                                          IID_PPV_ARGS(&m_CommandQueue)))) {
    std::fprintf(stderr, "[DX12] Failed to create command queue\n");
    return false;
  }

  return true;
}

bool DX12Renderer::CreateSwapChain(HWND hWnd) {
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.BufferCount = FRAME_COUNT;
  swapChainDesc.Width = m_Width;
  swapChainDesc.Height = m_Height;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.SampleDesc.Count = 1;

  ComPtr<IDXGISwapChain1> swapChain;
  if (FAILED(m_Factory->CreateSwapChainForHwnd(m_CommandQueue.Get(), hWnd,
                                               &swapChainDesc, nullptr, nullptr,
                                               &swapChain))) {
    std::fprintf(stderr, "[DX12] Failed to create swap chain\n");
    return false;
  }

  m_Factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
  swapChain.As(&m_SwapChain);
  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
  return true;
}

bool DX12Renderer::CreateRenderTargets() {
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = FRAME_COUNT;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  if (FAILED(m_Device->CreateDescriptorHeap(&rtvHeapDesc,
                                            IID_PPV_ARGS(&m_RtvHeap)))) {
    return false;
  }

  m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(
      m_RtvHeap->GetCPUDescriptorHandleForHeapStart());

  for (UINT n = 0; n < FRAME_COUNT; n++) {
    if (FAILED(m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_RenderTargets[n])))) {
      return false;
    }
    m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr,
                                     rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }
  return true;
}

bool DX12Renderer::CreateCommandObjects() {
  for (UINT n = 0; n < FRAME_COUNT; n++) {
    if (FAILED(m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_CommandAllocators[n])))) {
      return false;
    }
  }

  if (FAILED(
          m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      m_CommandAllocators[m_FrameIndex].Get(),
                                      nullptr, IID_PPV_ARGS(&m_CommandList)))) {
    return false;
  }
  m_CommandList->Close();
  return true;
}

bool DX12Renderer::CreateSyncObjects() {
  if (FAILED(m_Device->CreateFence(m_FenceValues[m_FrameIndex],
                                   D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(&m_Fence)))) {
    return false;
  }
  m_FenceValues[m_FrameIndex]++;

  m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!m_FenceEvent) {
    return false;
  }

  return true;
}

bool DX12Renderer::CreateTestGeometry() {
  // Quad vertices (Resident Evil style polygon with vertex color and UVs)
  PS1Vertex vertices[] = {
      {XMFLOAT3(-0.5f, 0.5f, 0.0f), XMFLOAT4(0.85f, 0.15f, 0.15f, 1.0f),
       XMFLOAT2(0.0f, 0.0f)}, // Blood Red
      {XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(0.15f, 0.85f, 0.15f, 1.0f),
       XMFLOAT2(1.0f, 1.0f)}, // Green Herb
      {XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(0.15f, 0.15f, 0.85f, 1.0f),
       XMFLOAT2(0.0f, 1.0f)}, // Blue Herb

      {XMFLOAT3(-0.5f, 0.5f, 0.0f), XMFLOAT4(0.85f, 0.15f, 0.15f, 1.0f),
       XMFLOAT2(0.0f, 0.0f)},
      {XMFLOAT3(0.5f, 0.5f, 0.0f), XMFLOAT4(0.85f, 0.85f, 0.20f, 1.0f),
       XMFLOAT2(1.0f, 0.0f)},
      {XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(0.15f, 0.85f, 0.15f, 1.0f),
       XMFLOAT2(1.0f, 1.0f)},
  };

  const UINT vertexBufferSize = sizeof(vertices);

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = vertexBufferSize;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  if (FAILED(m_Device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_VertexBuffer)))) {
    return false;
  }

  uint8_t *pVertexDataBegin = nullptr;
  D3D12_RANGE readRange = {0, 0};
  m_VertexBuffer->Map(0, &readRange,
                      reinterpret_cast<void **>(&pVertexDataBegin));
  std::memcpy(pVertexDataBegin, vertices, sizeof(vertices));
  m_VertexBuffer->Unmap(0, nullptr);

  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(PS1Vertex);
  m_VertexBufferView.SizeInBytes = vertexBufferSize;

  // Create 1x1 White Dummy Texture for untextured geometry
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.NumDescriptors = 2;
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(m_Device->CreateDescriptorHeap(&srvHeapDesc,
                                            IID_PPV_ARGS(&m_SrvHeap)))) {
    return false;
  }

  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.Width = 1;
  texDesc.Height = 1;
  texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  texDesc.DepthOrArraySize = 1;
  texDesc.SampleDesc.Count = 1;
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

  if (FAILED(m_Device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_DummyTexture)))) {
    return false;
  }

  uint32_t whitePixel = 0xFFFFFFFF;
  D3D12_SUBRESOURCE_DATA subData = {};
  subData.pData = &whitePixel;
  subData.RowPitch = 4;
  subData.SlicePitch = 4;

  D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
      m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
  UINT srvIncSize = m_Device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;
  m_Device->CreateShaderResourceView(m_DummyTexture.Get(), &srvDesc, srvHandle);
  srvHandle.ptr += srvIncSize;
  m_Device->CreateShaderResourceView(m_DummyTexture.Get(), &srvDesc, srvHandle);

  return true;
}

void DX12Renderer::BeginFrame() {
  m_CommandAllocators[m_FrameIndex]->Reset();
  m_CommandList->Reset(m_CommandAllocators[m_FrameIndex].Get(), nullptr);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  m_CommandList->ResourceBarrier(1, &barrier);

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(
      m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
  rtvHandle.ptr += m_FrameIndex * m_RtvDescriptorSize;

  m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
  m_CommandList->RSSetViewports(1, &m_Viewport);
  m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

  const float clearColor[] = {0.05f, 0.05f, 0.07f, 1.0f};
  m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void DX12Renderer::RenderTestTriangle(float angle) {
  m_Pipeline.Bind(m_CommandList.Get());

  // Update scene constants
  SceneConstants constants;
  constants.worldViewProj = XMMatrixTranspose(XMMatrixRotationZ(angle));
  constants.screenResolution = XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f,
                                        1.0f / 240.0f); // Authentic PS1 240p
  constants.renderFlags = XMFLOAT4(
      1.0f, 1.0f, 1.0f, 1.0f); // Snapping, Affine, Dither, 15-bit enabled
  constants.blendMode = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  m_Pipeline.UpdateConstants(m_CommandList.Get(), constants);

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  m_CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  m_CommandList->SetGraphicsRootDescriptorTable(
      1, m_SrvHeap->GetGPUDescriptorHandleForHeapStart());

  m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  m_CommandList->DrawInstanced(6, 1, 0, 0);
}

void DX12Renderer::EndFrame() {
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = m_RenderTargets[m_FrameIndex].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  m_CommandList->ResourceBarrier(1, &barrier);

  m_CommandList->Close();

  ID3D12CommandList *ppCommandLists[] = {m_CommandList.Get()};
  m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

  m_SwapChain->Present(1, 0);

  const uint64_t currentFenceValue = m_FenceValues[m_FrameIndex];
  m_CommandQueue->Signal(m_Fence.Get(), currentFenceValue);

  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

  if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex]) {
    m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent);
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
  }

  m_FenceValues[m_FrameIndex] = currentFenceValue + 1;
}

void DX12Renderer::WaitForGpu() {
  m_CommandQueue->Signal(m_Fence.Get(), m_FenceValues[m_FrameIndex]);
  m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent);
  WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
  m_FenceValues[m_FrameIndex]++;
}

void DX12Renderer::Shutdown() {
  WaitForGpu();
  m_Pipeline.Shutdown();
  if (m_FenceEvent) {
    CloseHandle(m_FenceEvent);
    m_FenceEvent = nullptr;
  }
}

void DX12Renderer::Resize(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0 || (width == m_Width && height == m_Height))
    return;

  WaitForGpu();
  for (UINT i = 0; i < FRAME_COUNT; i++) {
    m_RenderTargets[i].Reset();
  }

  m_Width = width;
  m_Height = height;
  m_Viewport = {
      0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height),
      0.0f, 1.0f};
  m_ScissorRect = {0, 0, static_cast<LONG>(m_Width),
                   static_cast<LONG>(m_Height)};

  m_SwapChain->ResizeBuffers(FRAME_COUNT, m_Width, m_Height,
                             DXGI_FORMAT_R8G8B8A8_UNORM, 0);
  m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

  CreateRenderTargets();
}

} // namespace Render
