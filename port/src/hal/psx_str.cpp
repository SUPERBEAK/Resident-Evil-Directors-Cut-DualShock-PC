#include <filesystem>
#include "psx_str.h"
#include "psx_mdec.h"
#include "ps1_pipeline.h"
#include <iostream>
#include <algorithm>
#include <cstring>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace PSX {

PSXMoviePlayer g_MoviePlayer;

static const float K0[5] = { 0.0f,  60.0f / 64.0f,  115.0f / 64.0f,  98.0f / 64.0f,  122.0f / 64.0f };
static const float K1[5] = { 0.0f,   0.0f,          -52.0f / 64.0f, -55.0f / 64.0f,  -60.0f / 64.0f };

#pragma pack(push, 1)
struct StrChunkHeader {
    uint16_t magic;
    uint16_t chunkType;
    uint16_t chunkNum;
    uint16_t chunkCount;
    uint32_t frameNum;
    uint32_t dataSize;
    uint16_t width;
    uint16_t height;
};
#pragma pack(pop)

static const char* s_MovieFiles[MOVIE_COUNT] = {
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/CAPCOM.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/STFR.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/PJ.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DM1.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DM2.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DM3.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DM4.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DM6.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DM7.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DM8.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DMB.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DMC.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DMD.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DME.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/DMF.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED1.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED2.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED3.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED4.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED5.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED6.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED7.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/ED8.STR",
    "extracted/RE1-Dualshock/PSXUSA/ZMOVIE/OJ.STR"
};

const char* PSXMoviePlayer::GetMovieFileName(MovieId id) {
    if (id >= 0 && id < MOVIE_COUNT) {
        return s_MovieFiles[id];
    }
    return "";
}

PSXMoviePlayer::PSXMoviePlayer() {
    m_CurrentRgba.resize(320 * 240, 0xFF000000);
}

PSXMoviePlayer::~PSXMoviePlayer() {
    Shutdown();
}

bool PSXMoviePlayer::Initialize(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, IXAudio2* pXAudio2) {
    m_Device = device;
    m_CmdQueue = cmdQueue;
    m_pXAudio2 = pXAudio2;
    if (!m_Device) return false;

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = 2;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_SrvHeap)))) {
        return false;
    }
    m_SrvGpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = 320;
    texDesc.Height = 240;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(m_Device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&m_MovieTexture)))) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvViewDesc = {};
    srvViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvViewDesc.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT incSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_Device->CreateShaderResourceView(m_MovieTexture.Get(), &srvViewDesc, cpuHandle);
    cpuHandle.ptr += incSize;
    m_Device->CreateShaderResourceView(m_MovieTexture.Get(), &srvViewDesc, cpuHandle);

    uint64_t uploadPitch = (320 * 4 + 255) & ~255;
    uint64_t uploadSize = uploadPitch * 240;

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = uploadSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_Device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_UploadBuffer)))) {
        return false;
    }

    Render::PS1Vertex quadVertices[] = {
        { XMFLOAT3(-1.0f,  1.0f, 0.999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 0.999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, 0.999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, 0.999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 1.0f,  1.0f, 0.999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 0.999f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) }
    };

    uint64_t vbSize = sizeof(quadVertices);
    bufDesc.Width = vbSize;
    if (FAILED(m_Device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_VertexBuffer)))) {
        return false;
    }

    void* pVbData = nullptr;
    if (SUCCEEDED(m_VertexBuffer->Map(0, nullptr, &pVbData))) {
        memcpy(pVbData, quadVertices, vbSize);
        m_VertexBuffer->Unmap(0, nullptr);
    }
    return true;
}

void PSXMoviePlayer::Shutdown() {
    Close();
    m_MovieTexture.Reset();
    m_UploadBuffer.Reset();
    m_VertexBuffer.Reset();
    m_SrvHeap.Reset();
}

bool PSXMoviePlayer::OpenMovie(MovieId id) {
    const char* path = GetMovieFileName(id);
    if (!path || !path[0]) return false;
    float fps = (id == MOVIE_CAPCOM) ? 20.0f : 15.0f;
    return Open(path, fps);
}

bool PSXMoviePlayer::Open(const std::string& filePath, float targetFps) {
    Close();
    std::string resolvedPath = filePath;
    if (!std::filesystem::exists(resolvedPath)) {
        std::string cand1 = std::string("../../../") + filePath;
        std::string cand2 = std::string("../../") + filePath;
        if (std::filesystem::exists(cand1)) {
            resolvedPath = cand1;
        } else if (std::filesystem::exists(cand2)) {
            resolvedPath = cand2;
        }
    }
    m_File.open(resolvedPath, std::ios::binary);
    if (!m_File.is_open()) {
        std::cerr << "[MoviePlayer] Failed to open movie file: " << filePath << std::endl;
        return false;
    }

    m_TargetFps = targetFps;
    m_FrameDuration = 1.0f / m_TargetFps;
    m_TimeAccumulator = 0.0f;
    m_CurrentFrameIndex = 0;
    m_HistL[0] = m_HistL[1] = 0;
    m_HistR[0] = m_HistR[1] = 0;
    m_AudioQueue.clear();
    m_HasFrameReady = false;

    if (m_pXAudio2) {
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = 37800;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;
        if (SUCCEEDED(m_pXAudio2->CreateSourceVoice(&m_pAudioVoice, &wfx))) {
            m_pAudioVoice->Start(0);
        }
    }

    DecodeNextFrame();
    m_IsPlaying = true;
    std::cout << "[MoviePlayer] Started playback: " << filePath << " at " << m_TargetFps << " FPS" << std::endl;
    return true;
}

void PSXMoviePlayer::Close() {
    if (m_File.is_open()) {
        m_File.close();
    }
    if (m_pAudioVoice) {
        m_pAudioVoice->Stop();
        m_pAudioVoice->FlushSourceBuffers();
        m_pAudioVoice->DestroyVoice();
        m_pAudioVoice = nullptr;
    }
    m_IsPlaying = false;
    m_HasFrameReady = false;
    m_AudioQueue.clear();
    m_StreamingChunk.clear();
}

void PSXMoviePlayer::DecodeXaSector(const uint8_t* sec) {
    for (int g = 0; g < 18; ++g) {
        const uint8_t* grp = sec + 8 + g * 128;
        uint8_t unitsParam[8] = { grp[0], grp[1], grp[2], grp[3], grp[8], grp[9], grp[10], grp[11] };
        int8_t unitSamples[8][28];
        for (int line = 0; line < 28; ++line) {
            const uint8_t* w = grp + 16 + line * 4;
            for (int b = 0; b < 4; ++b) {
                uint8_t val = w[b];
                int8_t lo = static_cast<int8_t>(val & 0x0F);
                if (lo >= 8) lo -= 16;
                int8_t hi = static_cast<int8_t>((val >> 4) & 0x0F);
                if (hi >= 8) hi -= 16;
                unitSamples[b * 2][line] = lo;
                unitSamples[b * 2 + 1][line] = hi;
            }
        }

        int16_t decodedL[112];
        int16_t decodedR[112];
        int lIdx = 0, rIdx = 0;

        for (int u = 0; u < 4; ++u) {
            int leftUnit = u * 2;
            int rightUnit = u * 2 + 1;
            int filtL = (unitsParam[leftUnit] >> 4) & 0x0F;
            int shiftL = unitsParam[leftUnit] & 0x0F;
            if (filtL > 4) filtL = 0;
            int filtR = (unitsParam[rightUnit] >> 4) & 0x0F;
            int shiftR = unitsParam[rightUnit] & 0x0F;
            if (filtR > 4) filtR = 0;

            for (int s = 0; s < 28; ++s) {
                int32_t nibL = unitSamples[leftUnit][s];
                int32_t sampleL = (12 - shiftL >= 0) ? (nibL << (12 - shiftL)) : (nibL >> (shiftL - 12));
                float predL = sampleL + K0[filtL] * m_HistL[0] + K1[filtL] * m_HistL[1];
                int32_t valL = std::clamp(static_cast<int32_t>(predL), -32768, 32767);
                m_HistL[1] = m_HistL[0];
                m_HistL[0] = valL;
                decodedL[lIdx++] = static_cast<int16_t>(valL);

                int32_t nibR = unitSamples[rightUnit][s];
                int32_t sampleR = (12 - shiftR >= 0) ? (nibR << (12 - shiftR)) : (nibR >> (shiftR - 12));
                float predR = sampleR + K0[filtR] * m_HistR[0] + K1[filtR] * m_HistR[1];
                int32_t valR = std::clamp(static_cast<int32_t>(predR), -32768, 32767);
                m_HistR[1] = m_HistR[0];
                m_HistR[0] = valR;
                decodedR[rIdx++] = static_cast<int16_t>(valR);
            }
        }

        for (int i = 0; i < 112; ++i) {
            m_AudioQueue.push_back(decodedL[i]);
            m_AudioQueue.push_back(decodedR[i]);
        }
    }
}

bool PSXMoviePlayer::DecodeNextFrame() {
    uint8_t sector[2336];
    while (m_File.read(reinterpret_cast<char*>(sector), sizeof(sector))) {
        uint8_t submode = sector[2];
        if (submode & 0x04) {
            DecodeXaSector(sector);
            continue;
        }
        if (submode & 0x02) {
            const StrChunkHeader* hdr = reinterpret_cast<const StrChunkHeader*>(sector + 8);
            if (hdr->magic != 0x0160) continue;
            if (hdr->chunkNum == 0) {
                m_FrameBits.clear();
                m_FrameBits.reserve(hdr->dataSize);
            }
            uint32_t bytesLeft = (hdr->dataSize > m_FrameBits.size()) ? (hdr->dataSize - static_cast<uint32_t>(m_FrameBits.size())) : 0;
            uint32_t chunkBytes = std::min<uint32_t>(2016, bytesLeft);
            if (chunkBytes > 0) {
                m_FrameBits.insert(m_FrameBits.end(), sector + 40, sector + 40 + chunkBytes);
            }
            if (hdr->chunkNum + 1 >= hdr->chunkCount) {
                if (m_FrameBits.size() >= 8) {
                    bool ok = MdecDecoder::DecodeBsFrame(m_FrameBits.data(), m_FrameBits.size(), m_CurrentRgba);
                    if (ok) {
                        m_HasFrameReady = true;
                        m_CurrentFrameIndex++;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool PSXMoviePlayer::Update(float deltaTime, uint32_t padButtons) {
    if (!m_IsPlaying) return false;
    static float s_Grace = 0.0f;
    s_Grace += deltaTime;
    if (s_Grace > 0.5f) {
        if ((padButtons & (1 << 0)) || (padButtons & (1 << 14)) ||
            (GetAsyncKeyState(VK_SPACE) & 0x8000) ||
            (GetAsyncKeyState(VK_RETURN) & 0x8000) ||
            (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
            std::cout << "Skipped by button" << std::endl;
            Close();
            s_Grace = 0.0f;
            return false;
        }
    }
    if (m_pAudioVoice && m_AudioQueue.size() >= 4032) {
        XAUDIO2_VOICE_STATE state = {};
        m_pAudioVoice->GetState(&state);
        if (state.BuffersQueued < 4) {
            m_StreamingChunk.assign(m_AudioQueue.begin(), m_AudioQueue.begin() + 4032);
            m_AudioQueue.erase(m_AudioQueue.begin(), m_AudioQueue.begin() + 4032);
            XAUDIO2_BUFFER xBuffer = {};
            xBuffer.AudioBytes = static_cast<UINT32>(m_StreamingChunk.size() * sizeof(int16_t));
            xBuffer.pAudioData = reinterpret_cast<const BYTE*>(m_StreamingChunk.data());
            m_pAudioVoice->SubmitSourceBuffer(&xBuffer);
        }
    }
    m_TimeAccumulator += deltaTime;
    while (m_TimeAccumulator >= m_FrameDuration) {
        m_TimeAccumulator -= m_FrameDuration;
        if (!DecodeNextFrame()) {
            Close();
            return false;
        }
    }
    return true;
}

void PSXMoviePlayer::UploadFrameTexture() {
    if (!m_HasFrameReady || !m_MovieTexture || !m_UploadBuffer) return;
    void* pMapped = nullptr;
    if (SUCCEEDED(m_UploadBuffer->Map(0, nullptr, &pMapped))) {
        uint8_t* dst = static_cast<uint8_t*>(pMapped);
        const uint8_t* src = reinterpret_cast<const uint8_t*>(m_CurrentRgba.data());
        uint32_t rowPitch = (320 * 4 + 255) & ~255;
        for (uint32_t y = 0; y < 240; ++y) {
            memcpy(dst + y * rowPitch, src + y * 320 * 4, 320 * 4);
        }
        m_UploadBuffer->Unmap(0, nullptr);
    }
}

void PSXMoviePlayer::Render(ID3D12GraphicsCommandList* cmdList, Render::PS1Pipeline& pipeline) {
    if (!m_IsPlaying || !m_HasFrameReady) return;
    UploadFrameTexture();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_MovieTexture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = m_MovieTexture.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = m_UploadBuffer.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset = 0;
    srcLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srcLoc.PlacedFootprint.Footprint.Width = 320;
    srcLoc.PlacedFootprint.Footprint.Height = 240;
    srcLoc.PlacedFootprint.Footprint.Depth = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = (320 * 4 + 255) & ~255;

    cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    Render::SceneConstants constants = {};
    constants.worldViewProj = XMMatrixIdentity();
    constants.prevWorldViewProj = XMMatrixIdentity();
    constants.screenResolution = XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
    constants.renderFlags = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    constants.blendMode = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    constants.jitterOffset = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    pipeline.Bind(cmdList);
    pipeline.UpdateConstants(cmdList, constants);
    cmdList->SetDescriptorHeaps(1, m_SrvHeap.GetAddressOf());
    cmdList->SetGraphicsRootDescriptorTable(1, m_SrvGpuHandle);

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
    vbView.StrideInBytes = sizeof(Render::PS1Vertex);
    vbView.SizeInBytes = sizeof(Render::PS1Vertex) * 6;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->DrawInstanced(6, 1, 0, 0);
}

} // namespace PSX
