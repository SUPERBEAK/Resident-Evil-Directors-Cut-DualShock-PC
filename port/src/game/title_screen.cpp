#include "title_screen.h"
#include "audio_engine.h"
#include "ps1_pipeline.h"
#include "psx_str.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace Game {

TitleScreen g_TitleScreen;

bool TitleScreen::Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                             const std::wstring &dataDir) {
  m_Device = device;
  m_CmdQueue = cmdQueue;

  D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
  srvDesc.NumDescriptors = kNumDescriptors;
  srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(m_Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_SrvHeap)))) {
    return false;
  }
  m_DescriptorSize = m_Device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC vbDesc = {};
  vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  vbDesc.Width = 24 * sizeof(Render::PS1Vertex);
  vbDesc.Height = 1;
  vbDesc.DepthOrArraySize = 1;
  vbDesc.MipLevels = 1;
  vbDesc.Format = DXGI_FORMAT_UNKNOWN;
  vbDesc.SampleDesc.Count = 1;
  vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  if (FAILED(m_Device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_VertexBuffer)))) {
    return false;
  }
  m_VertexBuffer->Map(0, nullptr, &m_VertexBufferMapped);
  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Render::PS1Vertex);
  m_VertexBufferView.SizeInBytes = static_cast<UINT>(vbDesc.Width);

  bool okWarn = LoadWarningTim(dataDir + L"/GWARNIN1.TIM");
  bool okTitle = LoadTitlePix(dataDir + L"/TITLE.PIX");
  bool okTmoji = LoadTmojiVariants(dataDir + L"/TMOJI.TIM");

  return okWarn && okTitle && okTmoji;
}

void TitleScreen::Shutdown() {
  if (m_VertexBuffer) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
    m_VertexBuffer.Reset();
  }
  for (uint32_t i = 0; i < kNumDescriptors; ++i) {
    m_Textures[i].Reset();
  }
  m_SrvHeap.Reset();
  m_Phase = Phase::Inactive;
}

bool TitleScreen::UploadTextureRGBA(const uint32_t *pixels, uint32_t width,
                                    uint32_t height,
                                    Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
                                    uint32_t descriptorIndex) {
  D3D12_HEAP_PROPERTIES defaultHeapProps = {};
  defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = width;
  texDesc.Height = height;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  outTex.Reset();
  if (FAILED(m_Device->CreateCommittedResource(
          &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outTex)))) {
    return false;
  }

  UINT64 uploadSize = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
  m_Device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, nullptr, nullptr,
                                  &uploadSize);

  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC uploadBufferDesc = {};
  uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  uploadBufferDesc.Width = uploadSize;
  uploadBufferDesc.Height = 1;
  uploadBufferDesc.DepthOrArraySize = 1;
  uploadBufferDesc.MipLevels = 1;
  uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  uploadBufferDesc.SampleDesc.Count = 1;
  uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  Microsoft::WRL::ComPtr<ID3D12Resource> texUpload;
  if (FAILED(m_Device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&texUpload)))) {
    return false;
  }

  uint8_t *mappedUpload = nullptr;
  texUpload->Map(0, nullptr, reinterpret_cast<void **>(&mappedUpload));
  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(mappedUpload + layout.Offset + y * layout.Footprint.RowPitch,
                pixels + y * width, width * 4);
  }
  texUpload->Unmap(0, nullptr);

  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
  m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   IID_PPV_ARGS(&cmdAlloc));
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(),
                              nullptr, IID_PPV_ARGS(&cmdList));

  D3D12_TEXTURE_COPY_LOCATION dst = {};
  dst.pResource = outTex.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = 0;

  D3D12_TEXTURE_COPY_LOCATION src = {};
  src.pResource = texUpload.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = layout;

  cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = outTex.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmdList->ResourceBarrier(1, &barrier);
  cmdList->Close();

  ID3D12CommandList *ppCmd[] = {cmdList.Get()};
  m_CmdQueue->ExecuteCommandLists(1, ppCmd);

  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  m_CmdQueue->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, hEvent);
  WaitForSingleObjectEx(hEvent, INFINITE, FALSE);
  CloseHandle(hEvent);

  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
      m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
  cpuHandle.ptr += static_cast<SIZE_T>(descriptorIndex) * m_DescriptorSize;

  D3D12_SHADER_RESOURCE_VIEW_DESC srvView = {};
  srvView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvView.Texture2D.MipLevels = 1;
  m_Device->CreateShaderResourceView(outTex.Get(), &srvView, cpuHandle);

  return true;
}

bool TitleScreen::LoadWarningTim(const std::wstring &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f.is_open()) return false;
  size_t sz = static_cast<size_t>(f.tellg());
  if (sz < 20 + 320 * 240 * 2) return false;
  f.seekg(20, std::ios::beg);
  std::vector<uint16_t> bgr(320 * 240);
  f.read(reinterpret_cast<char *>(bgr.data()), 320 * 240 * 2);

  std::vector<uint32_t> rgba(320 * 240);
  for (size_t i = 0; i < 320 * 240; ++i) {
    uint16_t c = bgr[i];
    uint32_t r = ((c & 0x1F) * 255) / 31;
    uint32_t g = (((c >> 5) & 0x1F) * 255) / 31;
    uint32_t b = (((c >> 10) & 0x1F) * 255) / 31;
    rgba[i] = (255u << 24) | (b << 16) | (g << 8) | r;
  }
  return UploadTextureRGBA(rgba.data(), 320, 240, m_Textures[0], 0);
}

bool TitleScreen::LoadTitlePix(const std::wstring &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f.is_open()) return false;
  size_t sz = static_cast<size_t>(f.tellg());
  if (sz < 320 * 240 * 2) return false;
  f.seekg(0, std::ios::beg);
  std::vector<uint16_t> bgr(320 * 240);
  f.read(reinterpret_cast<char *>(bgr.data()), 320 * 240 * 2);

  std::vector<uint32_t> rgba(320 * 240);
  for (size_t i = 0; i < 320 * 240; ++i) {
    uint16_t c = bgr[i];
    uint32_t r = ((c & 0x1F) * 255) / 31;
    uint32_t g = (((c >> 5) & 0x1F) * 255) / 31;
    uint32_t b = (((c >> 10) & 0x1F) * 255) / 31;
    rgba[i] = (255u << 24) | (b << 16) | (g << 8) | r;
  }
  return UploadTextureRGBA(rgba.data(), 320, 240, m_Textures[1], 1);
}

bool TitleScreen::LoadTmojiVariants(const std::wstring &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f.is_open()) return false;
  size_t sz = static_cast<size_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(sz);
  f.read(reinterpret_cast<char *>(data.data()), sz);

  // Locate 4-bit TIM blocks inside TMOJI.TIM
  std::vector<size_t> timOffsets;
  for (size_t off = 0; off + 32 < sz; off += 4) {
    uint32_t magic = *reinterpret_cast<const uint32_t *>(&data[off]);
    uint32_t flags = *reinterpret_cast<const uint32_t *>(&data[off + 4]);
    uint32_t bnum = *reinterpret_cast<const uint32_t *>(&data[off + 8]);
    if (magic == 0x10 && flags == 0x08 && (bnum == 0x2c || bnum == 0x8c)) {
      timOffsets.push_back(off);
      off += bnum;
    }
  }
  if (timOffsets.size() < 5) return false;

  auto decodeVariant = [&](size_t timOff, uint32_t clutRow,
                           uint32_t descIdx) -> bool {
    uint32_t clutBsize = *reinterpret_cast<const uint32_t *>(&data[timOff + 8]);
    const uint16_t *clut = reinterpret_cast<const uint16_t *>(
        &data[timOff + 20 + clutRow * 32]);
    size_t imgHdr = timOff + 8 + clutBsize;
    if (imgHdr + 12 >= sz) return false;
    uint16_t iw = *reinterpret_cast<const uint16_t *>(&data[imgHdr + 8]);
    uint16_t ih = *reinterpret_cast<const uint16_t *>(&data[imgHdr + 10]);
    uint32_t pw = static_cast<uint32_t>(iw) * 4;
    const uint16_t *words =
        reinterpret_cast<const uint16_t *>(&data[imgHdr + 12]);

    std::vector<uint32_t> rgba(pw * ih, 0);
    for (uint32_t y = 0; y < ih; ++y) {
      for (uint32_t x = 0; x < iw; ++x) {
        uint16_t w = words[y * iw + x];
        for (uint32_t n = 0; n < 4; ++n) {
          uint8_t idx = (w >> (n * 4)) & 0x0F;
          uint16_t c = clut[idx];
          if (c == 0) {
            rgba[y * pw + x * 4 + n] = 0;
          } else {
            uint32_t r = ((c & 0x1F) * 255) / 31;
            uint32_t g = (((c >> 5) & 0x1F) * 255) / 31;
            uint32_t b = (((c >> 10) & 0x1F) * 255) / 31;
            rgba[y * pw + x * 4 + n] = (255u << 24) | (b << 16) | (g << 8) | r;
          }
        }
      }
    }
    return UploadTextureRGBA(rgba.data(), pw, ih, m_Textures[descIdx], descIdx);
  };

  // 2: Press Any Button (TIM 0, CLUT 0)
  // 3..5: New Game / Load Game / Option (TIM 1, CLUT 0..2)
  // 6..8: Original / Beginner / Arrange (TIM 2..4, CLUT 0)
  bool ok = true;
  ok = ok && decodeVariant(timOffsets[0], 0, 2);
  ok = ok && decodeVariant(timOffsets[1], 0, 3);
  ok = ok && decodeVariant(timOffsets[1], 1, 4);
  ok = ok && decodeVariant(timOffsets[1], 2, 5);
  ok = ok && decodeVariant(timOffsets[2], 0, 6);
  ok = ok && decodeVariant(timOffsets[3], 0, 7);
  ok = ok && decodeVariant(timOffsets[4], 0, 8);
  return ok;
}

void TitleScreen::StartSequence(std::function<void()> onStartNewGame,
                                std::function<void()> onLoadGame) {
  m_OnStartNewGame = onStartNewGame;
  m_OnLoadGame = onLoadGame;
  m_Phase = Phase::GoreWarning;
  m_MenuPage = MenuPage::PressAnyButton;
  m_Timer = 0.0f;
  m_Brightness = 0.0f;
  m_MainSelection = 0;
  m_DifficultySelection = 0;
  m_SelectLoadGame = false;
  m_PrevButtons = 0xFFFFFFFFu;
  m_PrevActionDown = true;
  m_PrevCancelDown = true;
  m_PrevUpDown = true;
  m_PrevDownDown = true;
  g_AudioEngine.StopBgm();
}

void TitleScreen::SkipToGameplay() { m_Phase = Phase::Inactive; }

void TitleScreen::Update(float deltaTime, uint32_t buttons) {
  if (m_Phase == Phase::Inactive) return;

  uint32_t pressedButtons = buttons & ~m_PrevButtons;
  m_PrevButtons = buttons;

  bool rawAction = ((buttons & (1 << 0)) != 0) ||
                   ((buttons & (1 << 2)) != 0) ||
                   ((buttons & (1 << 3)) != 0) || // Start
                   ((buttons & (1 << 14)) != 0) ||
                   ((GetAsyncKeyState('E') & 0x8000) != 0) ||
                   ((GetAsyncKeyState('Z') & 0x8000) != 0) ||
                   ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ||
                   ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
  bool actionPressed =
      (rawAction && !m_PrevActionDown) ||
      ((pressedButtons & ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 14))) != 0);
  m_PrevActionDown = rawAction;

  bool rawCancel = ((buttons & (1 << 1)) != 0) ||
                   ((buttons & (1 << 13)) != 0) ||
                   ((GetAsyncKeyState('X') & 0x8000) != 0) ||
                   ((GetAsyncKeyState(VK_BACK) & 0x8000) != 0);
  bool cancelPressed = (rawCancel && !m_PrevCancelDown);
  m_PrevCancelDown = rawCancel;

  bool rawUp = ((buttons & (1 << 4)) != 0) ||
               ((GetAsyncKeyState('W') & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_UP) & 0x8000) != 0);
  bool rawDown = ((buttons & (1 << 5)) != 0) ||
                 ((GetAsyncKeyState('S') & 0x8000) != 0) ||
                 ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
  bool upPressed = rawUp && !m_PrevUpDown;
  bool downPressed = rawDown && !m_PrevDownDown;
  m_PrevUpDown = rawUp;
  m_PrevDownDown = rawDown;

  m_Timer += deltaTime;

  if (m_Phase == Phase::GoreWarning) {
    // 0.0..0.6s fade in, 0.6..2.8s hold, 2.8..3.4s fade out
    if (actionPressed && m_Timer < 2.8f) {
      m_Timer = 2.8f;
    }
    if (m_Timer < 0.6f) {
      m_Brightness = m_Timer / 0.6f;
    } else if (m_Timer < 2.8f) {
      m_Brightness = 1.0f;
    } else if (m_Timer < 3.4f) {
      m_Brightness = std::max(0.0f, 1.0f - (m_Timer - 2.8f) / 0.6f);
    } else {
      m_Phase = Phase::CapcomLogo;
      m_Timer = 0.0f;
      m_Brightness = 0.0f;
      if (!PSX::g_MoviePlayer.OpenMovie(PSX::MOVIE_CAPCOM)) {
        // Fallback directly to Title Menu if movie unavailable
        m_Phase = Phase::TitleMenu;
      }
    }
  } else if (m_Phase == Phase::CapcomLogo) {
    if (!PSX::g_MoviePlayer.IsPlaying()) {
      m_Phase = Phase::TitleMenu;
      m_MenuPage = MenuPage::PressAnyButton;
      m_Timer = 0.0f;
      m_Brightness = 0.0f;
    }
  } else if (m_Phase == Phase::TitleMenu) {
    m_Brightness = std::min(1.0f, m_Timer / 0.5f);

    if (m_MenuPage == MenuPage::PressAnyButton) {
      if (actionPressed) {
        g_AudioEngine.PlayMenuConfirm();
        m_MenuPage = MenuPage::MainMenu;
      }
    } else if (m_MenuPage == MenuPage::MainMenu) {
      if (upPressed) {
        m_MainSelection = (m_MainSelection + 2) % 3;
        g_AudioEngine.PlayMenuCursor();
      } else if (downPressed) {
        m_MainSelection = (m_MainSelection + 1) % 3;
        g_AudioEngine.PlayMenuCursor();
      } else if (actionPressed) {
        if (m_MainSelection == 0) {
          // New Game -> Difficulty Selection
          g_AudioEngine.PlayMenuConfirm();
          m_MenuPage = MenuPage::DifficultySelect;
        } else if (m_MainSelection == 1) {
          // Load Game
          g_AudioEngine.PlayMenuConfirm();
          g_AudioEngine.PlayTitleVoice();
          m_SelectLoadGame = true;
          m_Phase = Phase::StartingGame;
          m_Timer = 0.0f;
        } else {
          // Option -> Confirm sound & cycle back to New Game
          g_AudioEngine.PlayMenuConfirm();
          m_MainSelection = 0;
        }
      } else if (cancelPressed) {
        g_AudioEngine.PlayMenuCancel();
        m_MenuPage = MenuPage::PressAnyButton;
      }
    } else if (m_MenuPage == MenuPage::DifficultySelect) {
      if (upPressed) {
        m_DifficultySelection = (m_DifficultySelection + 2) % 3;
        g_AudioEngine.PlayMenuCursor();
      } else if (downPressed) {
        m_DifficultySelection = (m_DifficultySelection + 1) % 3;
        g_AudioEngine.PlayMenuCursor();
      } else if (actionPressed) {
        // Play iconic "Resident Evil!" announcer voice and start game!
        g_AudioEngine.PlayMenuConfirm();
        g_AudioEngine.PlayTitleVoice();
        m_SelectLoadGame = false;
        m_Phase = Phase::StartingGame;
        m_Timer = 0.0f;
      } else if (cancelPressed) {
        g_AudioEngine.PlayMenuCancel();
        m_MenuPage = MenuPage::MainMenu;
      }
    }
  } else if (m_Phase == Phase::StartingGame) {
    m_Brightness = std::max(0.0f, 1.0f - (m_Timer / 1.4f));
    if (m_Timer >= 1.5f) {
      m_Phase = Phase::Inactive;
      if (m_SelectLoadGame && m_OnLoadGame) {
        m_OnLoadGame();
      } else if (m_OnStartNewGame) {
        m_OnStartNewGame();
      }
    }
  }
}

void TitleScreen::Render(ID3D12GraphicsCommandList *cmdList,
                         Render::PS1Pipeline &pipeline) {
  if (m_Phase == Phase::Inactive || m_Phase == Phase::CapcomLogo ||
      !m_VertexBufferMapped || !m_SrvHeap) {
    return;
  }

  Render::PS1Vertex verts[12] = {};
  uint32_t numVerts = 6;

  auto fillQuad = [&](Render::PS1Vertex *dst, float x, float y, float w, float h,
                      float z, DirectX::XMFLOAT4 col) {
    float x0 = (x / 160.0f) - 1.0f;
    float y0 = 1.0f - (y / 120.0f);
    float x1 = ((x + w) / 160.0f) - 1.0f;
    float y1 = 1.0f - ((y + h) / 120.0f);

    dst[0] = {{x0, y0, z}, col, {0.0f, 0.0f}};
    dst[1] = {{x1, y0, z}, col, {1.0f, 0.0f}};
    dst[2] = {{x0, y1, z}, col, {0.0f, 1.0f}};
    dst[3] = {{x0, y1, z}, col, {0.0f, 1.0f}};
    dst[4] = {{x1, y0, z}, col, {1.0f, 0.0f}};
    dst[5] = {{x1, y1, z}, col, {1.0f, 1.0f}};
  };

  float b = std::clamp(m_Brightness, 0.0f, 1.0f);
  DirectX::XMFLOAT4 bgCol = {b, b, b, 1.0f};
  fillQuad(&verts[0], 0.0f, 0.0f, 320.0f, 240.0f, 0.5f, bgCol);

  uint32_t bgDescIdx = (m_Phase == Phase::GoreWarning) ? 0 : 1;
  uint32_t menuDescIdx = 2;

  if (m_Phase == Phase::TitleMenu || m_Phase == Phase::StartingGame) {
    fillQuad(&verts[6], 30.0f, 158.0f, 256.0f, 80.0f, 0.1f, bgCol);
    numVerts = 12;

    if (m_MenuPage == MenuPage::PressAnyButton) {
      menuDescIdx = 2;
    } else if (m_MenuPage == MenuPage::MainMenu) {
      menuDescIdx = 3 + static_cast<uint32_t>(m_MainSelection % 3);
    } else {
      menuDescIdx = 6 + static_cast<uint32_t>(m_DifficultySelection % 3);
    }
  }

  std::memcpy(m_VertexBufferMapped, verts, numVerts * sizeof(Render::PS1Vertex));

  pipeline.Bind(cmdList);
  Render::SceneConstants constants;
  constants.worldViewProj = DirectX::XMMatrixIdentity();
  constants.prevWorldViewProj = DirectX::XMMatrixIdentity();
  constants.screenResolution =
      DirectX::XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
  constants.blendMode = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  constants.jitterOffset = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  pipeline.UpdateConstants(cmdList, constants);

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  D3D12_GPU_DESCRIPTOR_HANDLE bgGpu =
      m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
  bgGpu.ptr += static_cast<UINT64>(bgDescIdx) * m_DescriptorSize;
  cmdList->SetGraphicsRootDescriptorTable(1, bgGpu);
  cmdList->DrawInstanced(6, 1, 0, 0);

  if (numVerts == 12) {
    constants.blendMode = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
    pipeline.UpdateConstants(cmdList, constants);

    D3D12_GPU_DESCRIPTOR_HANDLE menuGpu =
        m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
    menuGpu.ptr += static_cast<UINT64>(menuDescIdx) * m_DescriptorSize;
    cmdList->SetGraphicsRootDescriptorTable(1, menuGpu);
    cmdList->DrawInstanced(6, 1, 6, 0);
  }
}

} // namespace Game

