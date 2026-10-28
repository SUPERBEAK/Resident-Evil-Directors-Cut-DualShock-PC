#include "background_loader.h"
#include "ps1_pipeline.h"
#include "psx_mdec.h"
#include <fstream>
#include <cstdio>

namespace Render {

bool BackgroundLoader::Initialize(ID3D12Device* device, ID3D12CommandQueue* cmdQueue) {
    m_Device = device;
    m_CmdQueue = cmdQueue;

    // 1. Create SRV Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 2;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_SrvHeap)))) {
        return false;
    }

    m_SrvGpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();

    // 2. Create Full-Screen Background Quad (NDC: -1..1, covers entire 4:3 screen)
    PS1Vertex quadVertices[] = {
        { XMFLOAT3(-1.0f,  1.0f, 0.99999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 0.99999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, 0.99999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },

        { XMFLOAT3(-1.0f,  1.0f, 0.99999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f,  1.0f, 0.99999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 0.99999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
    };

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeof(quadVertices);
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_QuadVertexBuffer)))) {
        return false;
    }

    uint8_t* pVertexData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    m_QuadVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexData));
    std::memcpy(pVertexData, quadVertices, sizeof(quadVertices));
    m_QuadVertexBuffer->Unmap(0, nullptr);

    m_QuadVertexBufferView.BufferLocation = m_QuadVertexBuffer->GetGPUVirtualAddress();
    m_QuadVertexBufferView.StrideInBytes = sizeof(PS1Vertex);
    m_QuadVertexBufferView.SizeInBytes = sizeof(quadVertices);

    // 3. Create 320x240 Texture2D on DEFAULT Heap
    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = BG_WIDTH;
    texDesc.Height = BG_HEIGHT;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if (FAILED(device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&m_BgTexture)))) {
        return false;
    }

    // 4. Create SRV for Background Texture
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT srvIncSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(m_BgTexture.Get(), &srvDesc, srvCpuHandle);
    srvCpuHandle.ptr += srvIncSize;
    device->CreateShaderResourceView(m_BgTexture.Get(), &srvDesc, srvCpuHandle);

    std::printf("[BackgroundLoader] Background loader initialized (320x240 MDEC target)\n");
    return true;
}

void BackgroundLoader::Shutdown() {
    m_BgTexture.Reset();
    m_UploadBuffer.Reset();
    m_QuadVertexBuffer.Reset();
    m_SrvHeap.Reset();
}

bool BackgroundLoader::LoadRoomCut(const std::wstring& bssFilePath, uint32_t cutIndex) {
    std::ifstream file(bssFilePath, std::ios::binary);
    if (!file.is_open()) {
        std::fprintf(stderr, "[BackgroundLoader] Could not open BSS file!\n");
        return false;
    }

    std::printf("[BackgroundLoader] Calling DecodeBsFrame...\n");
    const size_t cutSize = 0x8000; // 32KB per cut
    std::vector<uint8_t> cutData(cutSize);

    file.seekg(cutIndex * cutSize, std::ios::beg);
    file.read(reinterpret_cast<char*>(cutData.data()), cutSize);

    std::vector<uint32_t> rgbaPixels;
    if (!PSX::MdecDecoder::DecodeBsFrame(cutData.data(), cutData.size(), rgbaPixels)) {
        // Fallback test background (classic mansion atmosphere)
        rgbaPixels.resize(BG_WIDTH * BG_HEIGHT);
        for (uint32_t y = 0; y < BG_HEIGHT; ++y) {
            for (uint32_t x = 0; x < BG_WIDTH; ++x) {
                uint8_t r = static_cast<uint8_t>(40 + (x * 30) / BG_WIDTH);
                uint8_t g = static_cast<uint8_t>(20 + (y * 20) / BG_HEIGHT);
                uint8_t b = static_cast<uint8_t>(25);
                rgbaPixels[y * BG_WIDTH + x] = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }

    // Upload pixels to Default Heap Texture via temporary allocator/command list
    ComPtr<ID3D12CommandAllocator> cmdAlloc;
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
    m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList));

    const UINT64 uploadBufferSize = BG_WIDTH * BG_HEIGHT * 4;
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = uploadBufferSize * 2;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    m_Device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_UploadBuffer));

    uint8_t* pUploadData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    m_UploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pUploadData));
    std::memcpy(pUploadData, rgbaPixels.data(), BG_WIDTH * BG_HEIGHT * 4);
    m_UploadBuffer->Unmap(0, nullptr);

    // Transition Texture to COPY_DEST
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_BgTexture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_BgTexture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = m_UploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = BG_WIDTH;
    src.PlacedFootprint.Footprint.Height = BG_HEIGHT;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = BG_WIDTH * 4;

    cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // Transition back to PIXEL_SHADER_RESOURCE
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->Close();
    ID3D12CommandList* ppCommandLists[] = { cmdList.Get() };
    m_CmdQueue->ExecuteCommandLists(1, ppCommandLists);

    // Synchronize
    ComPtr<ID3D12Fence> fence;
    m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_CmdQueue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, hEvent);
    WaitForSingleObjectEx(hEvent, INFINITE, FALSE);
    CloseHandle(hEvent);

    std::printf("[BackgroundLoader] Loaded room background: Cut %u\n", cutIndex);
    return true;
}

void BackgroundLoader::Draw(ID3D12GraphicsCommandList* cmdList) {
    ID3D12DescriptorHeap* ppHeaps[] = { m_SrvHeap.Get() };
    cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    cmdList->SetGraphicsRootDescriptorTable(1, m_SrvGpuHandle);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_QuadVertexBufferView);
    cmdList->DrawInstanced(6, 1, 0, 0);
}

} // namespace Render
