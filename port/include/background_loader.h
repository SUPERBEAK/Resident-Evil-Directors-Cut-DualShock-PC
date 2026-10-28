#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace Render {

// Loader and DX12 resource manager for PS1 BSS pre-rendered backgrounds
class BackgroundLoader {
public:
    static constexpr uint32_t BG_WIDTH = 320;
    static constexpr uint32_t BG_HEIGHT = 240;

    BackgroundLoader() = default;
    ~BackgroundLoader() = default;

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* cmdQueue);
    void Shutdown();

    // Loads a camera cut (0-indexed) from an extracted BSS file (e.g. "STAGE1/ROOM100.BSS")
    bool LoadRoomCut(const std::wstring& bssFilePath, uint32_t cutIndex);

    ID3D12Resource* GetTexture() const { return m_BgTexture.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const { return m_SrvGpuHandle; }

    void Draw(ID3D12GraphicsCommandList* cmdList);

private:
    ID3D12Device* m_Device = nullptr;
    ID3D12CommandQueue* m_CmdQueue = nullptr;

    ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpuHandle{};
    ComPtr<ID3D12Resource> m_BgTexture;
    ComPtr<ID3D12Resource> m_UploadBuffer;

    ComPtr<ID3D12Resource> m_QuadVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_QuadVertexBufferView{};
};

} // namespace Render
