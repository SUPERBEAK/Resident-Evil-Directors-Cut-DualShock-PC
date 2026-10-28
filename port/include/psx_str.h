#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <xaudio2.h>

namespace Render {
    class PS1Pipeline;
}

namespace PSX {

enum MovieId {
    MOVIE_CAPCOM = 0,
    MOVIE_INTRO = 1,      // STFR.STR (or PJ.STR)
    MOVIE_PROLOGUE = 2,   // PJ.STR
    MOVIE_DEATH_1 = 3,    // DM1.STR
    MOVIE_DEATH_2 = 4,    // DM2.STR
    MOVIE_DEATH_3 = 5,    // DM3.STR
    MOVIE_DEATH_4 = 6,    // DM4.STR
    MOVIE_DEATH_6 = 7,    // DM6.STR
    MOVIE_DEATH_7 = 8,    // DM7.STR
    MOVIE_DEATH_8 = 9,    // DM8.STR
    MOVIE_DEATH_B = 10,   // DMB.STR
    MOVIE_DEATH_C = 11,   // DMC.STR
    MOVIE_DEATH_D = 12,   // DMD.STR
    MOVIE_DEATH_E = 13,   // DME.STR
    MOVIE_DEATH_F = 14,   // DMF.STR
    MOVIE_ENDING_1 = 15,  // ED1.STR
    MOVIE_ENDING_2 = 16,  // ED2.STR
    MOVIE_ENDING_3 = 17,  // ED3.STR
    MOVIE_ENDING_4 = 18,  // ED4.STR
    MOVIE_ENDING_5 = 19,  // ED5.STR
    MOVIE_ENDING_6 = 20,  // ED6.STR
    MOVIE_ENDING_7 = 21,  // ED7.STR
    MOVIE_ENDING_8 = 22,  // ED8.STR
    MOVIE_OJ = 23,        // OJ.STR
    MOVIE_COUNT = 24
};

class PSXMoviePlayer {
public:
    PSXMoviePlayer();
    ~PSXMoviePlayer();

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, IXAudio2* pXAudio2);
    void Shutdown();

    bool Open(const std::string& filePath, float targetFps = 15.0f);
    bool OpenMovie(MovieId id);
    void Close();

    bool Update(float deltaTime, uint32_t padButtons);
    void Render(ID3D12GraphicsCommandList* cmdList, Render::PS1Pipeline& pipeline);

    bool IsPlaying() const { return m_IsPlaying; }
    void Skip() { Close(); }

    static const char* GetMovieFileName(MovieId id);

private:
    bool DecodeNextFrame();
    void DecodeXaSector(const uint8_t* sec);
    void UploadFrameTexture();

    ID3D12Device* m_Device = nullptr;
    ID3D12CommandQueue* m_CmdQueue = nullptr;
    IXAudio2* m_pXAudio2 = nullptr;
    IXAudio2SourceVoice* m_pAudioVoice = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpuHandle = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_MovieTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_UploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;

    std::ifstream m_File;
    bool m_IsPlaying = false;
    float m_TargetFps = 15.0f;
    float m_FrameDuration = 1.0f / 15.0f;
    float m_TimeAccumulator = 0.0f;
    uint32_t m_CurrentFrameIndex = 0;

    std::vector<uint8_t> m_FrameBits;
    std::vector<uint32_t> m_CurrentRgba;
    bool m_HasFrameReady = false;

    std::vector<int16_t> m_AudioQueue;
    std::vector<int16_t> m_StreamingChunk;
    int32_t m_HistL[2] = {0, 0};
    int32_t m_HistR[2] = {0, 0};
};

extern PSXMoviePlayer g_MoviePlayer;

} // namespace PSX
