#include "text_box.h"
#include "audio_engine.h"
#include "ps1_pipeline.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace Game {

TextBox g_TextBox;

static const uint8_t s_Font8x8[95][8] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
  {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00},
  {0x18, 0x7E, 0xC0, 0x7C, 0x06, 0x7E, 0x18, 0x00},
  {0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00},
  {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00},
  {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
  {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
  {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
  {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
  {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
  {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00},
  {0x3C, 0x66, 0xC3, 0xC3, 0xC3, 0x66, 0x3C, 0x00},
  {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
  {0x7C, 0xC6, 0x06, 0x1C, 0x30, 0x60, 0xFE, 0x00},
  {0x7E, 0x0C, 0x18, 0x3C, 0x06, 0xC6, 0x7C, 0x00},
  {0x0C, 0x1C, 0x34, 0x64, 0xFE, 0x04, 0x04, 0x00},
  {0xFE, 0xC0, 0xFC, 0x06, 0x06, 0xC6, 0x7C, 0x00},
  {0x38, 0x60, 0xC0, 0xFC, 0xC6, 0xC6, 0x7C, 0x00},
  {0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
  {0x7C, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0x7C, 0x00},
  {0x7C, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0x78, 0x00},
  {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
  {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00},
  {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
  {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
  {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00},
  {0x7C, 0xC6, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x00},
  {0x7C, 0xC6, 0xDE, 0xDE, 0xDC, 0xC0, 0x7C, 0x00},
  {0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0x00},
  {0xFC, 0x66, 0x66, 0x7C, 0x66, 0x66, 0xFC, 0x00},
  {0x3C, 0x66, 0xC0, 0xC0, 0xC0, 0x66, 0x3C, 0x00},
  {0xF8, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00},
  {0xFE, 0x62, 0x68, 0x78, 0x68, 0x62, 0xFE, 0x00},
  {0xFE, 0x62, 0x68, 0x78, 0x68, 0x60, 0xF0, 0x00},
  {0x3C, 0x66, 0xC0, 0xC0, 0xCE, 0x66, 0x3E, 0x00},
  {0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00},
  {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
  {0x1E, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00},
  {0xE6, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0xE6, 0x00},
  {0xF0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00},
  {0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00},
  {0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00},
  {0x38, 0x6C, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00},
  {0xFC, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00},
  {0x38, 0x6C, 0xC6, 0xC6, 0xD6, 0x6C, 0x3A, 0x00},
  {0xFC, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0xE6, 0x00},
  {0x7C, 0xC6, 0x60, 0x38, 0x0C, 0xC6, 0x7C, 0x00},
  {0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
  {0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00},
  {0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00},
  {0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00},
  {0xC6, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0xC6, 0x00},
  {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x00},
  {0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x62, 0xFE, 0x00},
  {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
  {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00},
  {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
  {0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
  {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00},
  {0xE0, 0x60, 0x7C, 0x66, 0x66, 0x66, 0xDC, 0x00},
  {0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC6, 0x7C, 0x00},
  {0x1C, 0x0C, 0x7C, 0xCC, 0xCC, 0xCC, 0x76, 0x00},
  {0x00, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0x7C, 0x00},
  {0x1C, 0x36, 0x30, 0x78, 0x30, 0x30, 0x78, 0x00},
  {0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8},
  {0xE0, 0x60, 0x6C, 0x76, 0x66, 0x66, 0xE6, 0x00},
  {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
  {0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C},
  {0xE0, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0xE6, 0x00},
  {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
  {0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xD6, 0x00},
  {0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x00},
  {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x00},
  {0x00, 0x00, 0xDC, 0x66, 0x66, 0x7C, 0x60, 0xF0},
  {0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0x1E},
  {0x00, 0x00, 0xDC, 0x76, 0x60, 0x60, 0xF0, 0x00},
  {0x00, 0x00, 0x7E, 0xC0, 0x7C, 0x06, 0xFC, 0x00},
  {0x30, 0x30, 0x7C, 0x30, 0x30, 0x34, 0x18, 0x00},
  {0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00},
  {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00},
  {0x00, 0x00, 0xC6, 0xD6, 0xFE, 0xEE, 0x6C, 0x00},
  {0x00, 0x00, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x00},
  {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0xFC},
  {0x00, 0x00, 0xFE, 0x8C, 0x18, 0x32, 0xFE, 0x00},
  {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00},
  {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},
  {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00},
  {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};


std::string TextBox::DecodeCapcomString(const uint8_t *data, size_t maxLen,
                                       const std::string &itemInsert) {
  std::string result;
  for (size_t i = 0; i < maxLen; ++i) {
    uint8_t b = data[i];
    if (b == 0xFF || b == 0xFE) break;
    if (b == 0) {
      result += ' ';
    } else if (b == 1) {
      result += '?';
    } else if (b == 2) {
      result += '\n';
    } else if (b == 3) {
      result += '!';
    } else if (b == 5) {
      if (!itemInsert.empty()) {
        result += itemInsert;
      } else {
        result += "ITEM";
      }
    } else if (b == 6) {
      result += '.';
    } else if (b == 7) {
      result += ',';
    } else if (b == 8) {
      result += ':';
    } else if (b == 9) {
      result += '-';
    } else if (b == 10 || b == 27) {
      // Formatting / color control codes: skip
      continue;
    } else if (b >= 17 && b <= 26) {
      result += static_cast<char>('0' + (b - 17));
    } else if (b >= 29 && b <= 54) {
      result += static_cast<char>('A' + (b - 29));
    } else if (b >= 61 && b <= 86) {
      result += static_cast<char>('a' + (b - 61));
    } else if (b == 36) {
      result += '/';
    } else if (b == 37) {
      result += '(';
    } else if (b == 38) {
      result += ')';
    } else if (b < 32) {
      // Skip unrecognized control byte
    } else {
      result += static_cast<char>(b);
    }
  }
  return result;
}

bool TextBox::Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                         const std::wstring &fontTimPath,
                         const std::wstring &slusPath) {
  m_Device = device;
  m_CmdQueue = cmdQueue;

  // 1. Build Font Atlas (512x512)
  constexpr uint32_t atlasW = 512;
  constexpr uint32_t atlasH = 512;
  std::vector<uint32_t> atlasPixels(atlasW * atlasH, 0);

  // Load FONT.TIM if available
  bool loadedTim = false;
  if (!fontTimPath.empty() && std::filesystem::exists(fontTimPath)) {
    std::ifstream timFile(fontTimPath, std::ios::binary | std::ios::ate);
    if (timFile.is_open()) {
      size_t timSize = static_cast<size_t>(timFile.tellg());
      timFile.seekg(0, std::ios::beg);
      std::vector<uint8_t> timData(timSize);
      timFile.read(reinterpret_cast<char *>(timData.data()), timSize);

      if (timSize > 32 && *reinterpret_cast<const uint32_t *>(timData.data()) == 0x10) {
        uint32_t clutBsize = *reinterpret_cast<const uint32_t *>(timData.data() + 8);
        const uint16_t *clutWords = reinterpret_cast<const uint16_t *>(timData.data() + 20);
        uint32_t numColors = (clutBsize - 12) / 2;
        std::vector<uint32_t> palette(numColors);
        for (uint32_t i = 0; i < numColors; ++i) {
          uint16_t c = clutWords[i];
          uint32_t r = ((c & 0x1F) * 255) / 31;
          uint32_t g = (((c >> 5) & 0x1F) * 255) / 31;
          uint32_t b = (((c >> 10) & 0x1F) * 255) / 31;
          uint32_t a = (c == 0) ? 0 : 255;
          palette[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }

        size_t imgHdr = 8 + clutBsize;
        if (imgHdr + 12 < timSize) {
          uint16_t iw = *reinterpret_cast<const uint16_t *>(timData.data() + imgHdr + 8);
          uint16_t ih = *reinterpret_cast<const uint16_t *>(timData.data() + imgHdr + 10);
          uint32_t pixW = iw * 4; // 4-bit indexed
          const uint8_t *pixData = timData.data() + imgHdr + 12;

          for (uint32_t y = 0; y < ih && y < 256; ++y) {
            for (uint32_t x = 0; x < pixW && x < 256; ++x) {
              uint8_t byteVal = pixData[y * (iw * 2) + (x / 2)];
              uint8_t idx = (x % 2 == 0) ? (byteVal & 0x0F) : (byteVal >> 4);
              if (idx < palette.size()) {
                atlasPixels[y * atlasW + x] = palette[idx];
              }
            }
          }
          loadedTim = true;
        }
      }
    }
  }

  // Bake 8x8 fallback font glyphs at (0..127, 256..303)
  for (int g = 0; g < 95; ++g) {
    int col = g % 16;
    int row = g / 16;
    int gx = col * 8;
    int gy = 256 + row * 8;
    for (int y = 0; y < 8; ++y) {
      uint8_t rowBits = s_Font8x8[g][y];
      for (int x = 0; x < 8; ++x) {
        if ((rowBits >> (7 - x)) & 1) {
          atlasPixels[(gy + y) * atlasW + (gx + x)] = 0xFFFFFFFF;
        }
      }
    }
  }

  // Solid white pixel at (511, 511)
  atlasPixels[511 * atlasW + 511] = 0xFFFFFFFF;

  // 2. Upload Atlas to D3D12 Texture
  D3D12_HEAP_PROPERTIES defaultHeapProps = {};
  defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = atlasW;
  texDesc.Height = atlasH;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  HRESULT hr = m_Device->CreateCommittedResource(
      &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&m_FontTexture));
  if (FAILED(hr)) return false;

  UINT64 uploadSize = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
  m_Device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, nullptr, nullptr, &uploadSize);

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
  m_Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
                                   &uploadBufferDesc,
                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                   IID_PPV_ARGS(&texUpload));

  uint8_t *mappedUpload = nullptr;
  texUpload->Map(0, nullptr, reinterpret_cast<void **>(&mappedUpload));
  for (uint32_t y = 0; y < atlasH; ++y) {
    std::memcpy(mappedUpload + layout.Offset + y * layout.Footprint.RowPitch,
                atlasPixels.data() + y * atlasW, atlasW * 4);
  }
  texUpload->Unmap(0, nullptr);

  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
  m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   IID_PPV_ARGS(&cmdAlloc));
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(),
                              nullptr, IID_PPV_ARGS(&cmdList));

  D3D12_TEXTURE_COPY_LOCATION dst = {};
  dst.pResource = m_FontTexture.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = 0;

  D3D12_TEXTURE_COPY_LOCATION src = {};
  src.pResource = texUpload.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = layout;

  cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_FontTexture.Get();
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

  // 3. Create SRV Descriptor Heap
  D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
  srvDesc.NumDescriptors = 1;
  srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  m_Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_SrvHeap));

  D3D12_SHADER_RESOURCE_VIEW_DESC srvView = {};
  srvView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvView.Texture2D.MipLevels = 1;
  m_Device->CreateShaderResourceView(
      m_FontTexture.Get(), &srvView,
      m_SrvHeap->GetCPUDescriptorHandleForHeapStart());
  m_SrvGpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();

  // 4. Create Dynamic Vertex Buffer (Max 2048 vertices)
  constexpr UINT maxVerts = 2048;
  D3D12_RESOURCE_DESC vbDesc = {};
  vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  vbDesc.Width = maxVerts * sizeof(Render::PS1Vertex);
  vbDesc.Height = 1;
  vbDesc.DepthOrArraySize = 1;
  vbDesc.MipLevels = 1;
  vbDesc.Format = DXGI_FORMAT_UNKNOWN;
  vbDesc.SampleDesc.Count = 1;
  vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  m_Device->CreateCommittedResource(
      &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&m_VertexBuffer));
  m_VertexBuffer->Map(0, nullptr, &m_VertexBufferMapped);
  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Render::PS1Vertex);
  m_VertexBufferView.SizeInBytes = static_cast<UINT>(vbDesc.Width);

  // 5. Pre-cache Global Strings from SLUS_007.47
  if (!slusPath.empty() && std::filesystem::exists(slusPath)) {
    std::ifstream slusFile(slusPath, std::ios::binary | std::ios::ate);
    if (slusFile.is_open()) {
      size_t slusSize = static_cast<size_t>(slusFile.tellg());
      slusFile.seekg(0, std::ios::beg);
      std::vector<uint8_t> slusData(slusSize);
      slusFile.read(reinterpret_cast<char *>(slusData.data()), slusSize);

      constexpr uint32_t tableOffset = 0x84CBC;
      if (tableOffset + 256 <= slusSize) {
        for (uint32_t i = 0; i < 64; ++i) {
          uint32_t ptr = *reinterpret_cast<const uint32_t *>(
              slusData.data() + tableOffset + i * 4);
          if (ptr >= 0x8000F800 && (ptr - 0x8000F800) < slusSize) {
            uint32_t fileOff = ptr - 0x8000F800;
            std::string msg = DecodeCapcomString(
                slusData.data() + fileOff, std::min<size_t>(300, slusSize - fileOff));
            m_GlobalMessages.push_back(msg);
          } else {
            m_GlobalMessages.push_back("");
          }
        }
      }
    }
  }

  return true;
}

void TextBox::Shutdown() {
  if (m_VertexBuffer) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
    m_VertexBuffer.Reset();
  }
  m_FontTexture.Reset();
  m_SrvHeap.Reset();
  m_IsActive = false;
}

void TextBox::ShowMessage(const std::string &message, bool isPrompt,
                         std::function<void(bool)> callback) {
  m_Pages.clear();
  m_CurrentPage = 0;
  m_TypewriterTimer = 0.0f;
  m_CharsRevealed = 0;
  m_PageFullyRevealed = false;
  m_IsPrompt = isPrompt;
  m_PromptSelection = 0;
  m_Callback = callback;
  m_BlinkTimer = 0.0f;
  // Latch held buttons when opening so the triggering press never auto-dismisses the text box
  m_PrevButtons = 0xFFFFFFFFu;
  m_PrevActionDown = true;
  m_PrevUpDown = true;
  m_PrevDownDown = true;

  // Split long messages into pages by triple-newline or character count
  std::string curPage = "";
  int lineCount = 0;
  for (size_t i = 0; i < message.size(); ++i) {
    if (message[i] == '\n') {
      lineCount++;
      if (lineCount >= 3) {
        m_Pages.push_back(curPage);
        curPage = "";
        lineCount = 0;
        continue;
      }
    }
    curPage += message[i];
  }
  if (!curPage.empty()) {
    m_Pages.push_back(curPage);
  }
  if (m_Pages.empty()) {
    m_Pages.push_back(message);
  }

  m_IsActive = true;
}

void TextBox::ShowRoomMessage(uint32_t messageIndex,
                             const std::vector<uint8_t> &rdtData,
                             bool isPrompt,
                             std::function<void(bool)> callback) {
  if (rdtData.size() <= 0x78) return;
  uint32_t msgSection = *reinterpret_cast<const uint32_t *>(&rdtData[0x74]);
  if (msgSection == 0 || msgSection >= rdtData.size()) return;

  uint16_t firstOff = *reinterpret_cast<const uint16_t *>(&rdtData[msgSection]);
  uint32_t count = firstOff / 2;
  if (messageIndex >= count) return;

  uint16_t off = *reinterpret_cast<const uint16_t *>(
      &rdtData[msgSection + messageIndex * 2]);
  if (msgSection + off >= rdtData.size()) return;

  const uint8_t *msgPtr = &rdtData[msgSection + off];
  size_t maxLen = rdtData.size() - (msgSection + off);
  std::string decoded = DecodeCapcomString(msgPtr, std::min<size_t>(512, maxLen));

  ShowMessage(decoded, isPrompt, callback);
}

void TextBox::Close() {
  m_IsActive = false;
  if (m_Callback) {
    auto cb = m_Callback;
    m_Callback = nullptr;
    cb(m_PromptSelection == 0);
  }
}

void TextBox::AdvancePage() {
  if (m_CurrentPage + 1 < m_Pages.size()) {
    m_CurrentPage++;
    m_CharsRevealed = 0;
    m_PageFullyRevealed = false;
    m_TypewriterTimer = 0.0f;
  } else {
    Close();
  }
}

void TextBox::Update(float deltaTime, uint32_t buttons) {
  uint32_t pressedButtons = buttons & ~m_PrevButtons;
  m_PrevButtons = buttons;
  if (!m_IsActive) return;

  m_BlinkTimer += deltaTime;
  if (!m_PageFullyRevealed) {
    m_TypewriterTimer += deltaTime;
    // Reveal ~120 characters per second (2 chars per 60fps frame)
    m_CharsRevealed = static_cast<size_t>(m_TypewriterTimer * 120.0f);
    if (m_CurrentPage < m_Pages.size() &&
        m_CharsRevealed >= m_Pages[m_CurrentPage].size()) {
      m_CharsRevealed = m_Pages[m_CurrentPage].size();
      m_PageFullyRevealed = true;
    }
  }

  // Rising-edge detection across both gamepad buttons and keyboard keys
  bool rawAction = ((buttons & (1 << 0)) != 0) ||
                   ((buttons & (1 << 2)) != 0) ||
                   ((buttons & (1 << 14)) != 0) ||
                   ((GetAsyncKeyState('E') & 0x8000) != 0) ||
                   ((GetAsyncKeyState('Z') & 0x8000) != 0) ||
                   ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ||
                   ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
  bool actionPressed = (rawAction && !m_PrevActionDown) ||
                       ((pressedButtons & ((1 << 0) | (1 << 2) | (1 << 14))) != 0);
  m_PrevActionDown = rawAction;

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

  // Up/Down selection for Yes/No prompt
  if (m_IsPrompt && m_PageFullyRevealed) {
    if (upPressed) {
      if (m_PromptSelection != 0) {
        m_PromptSelection = 0;
        g_AudioEngine.PlayMenuCursor();
      }
    } else if (downPressed) {
      if (m_PromptSelection != 1) {
        m_PromptSelection = 1;
        g_AudioEngine.PlayMenuCursor();
      }
    }
  }

  if (actionPressed) {
    if (!m_PageFullyRevealed) {
      // Instantly reveal current page
      if (m_CurrentPage < m_Pages.size()) {
        m_CharsRevealed = m_Pages[m_CurrentPage].size();
      }
      m_PageFullyRevealed = true;
    } else {
      AdvancePage();
    }
  }
}

void TextBox::RebuildVertices() {
  if (!m_IsActive || !m_VertexBufferMapped || m_CurrentPage >= m_Pages.size()) {
    m_VertexCount = 0;
    return;
  }

  std::vector<Render::PS1Vertex> verts;
  auto AddQuad = [&](float x, float y, float w, float h, float u0, float v0,
                     float u1, float v1, DirectX::XMFLOAT4 col) {
    float x0 = (x / 160.0f) - 1.0f;
    float y0 = 1.0f - (y / 120.0f);
    float x1 = ((x + w) / 160.0f) - 1.0f;
    float y1 = 1.0f - ((y + h) / 120.0f);

    float tx0 = u0 / 512.0f;
    float ty0 = v0 / 512.0f;
    float tx1 = u1 / 512.0f;
    float ty1 = v1 / 512.0f;

    Render::PS1Vertex v[4];
    v[0].position = DirectX::XMFLOAT3(x0, y0, 0.05f);
    v[0].uv = DirectX::XMFLOAT2(tx0, ty0);
    v[0].color = col;

    v[1].position = DirectX::XMFLOAT3(x1, y0, 0.05f);
    v[1].uv = DirectX::XMFLOAT2(tx1, ty0);
    v[1].color = col;

    v[2].position = DirectX::XMFLOAT3(x0, y1, 0.05f);
    v[2].uv = DirectX::XMFLOAT2(tx0, ty1);
    v[2].color = col;

    v[3].position = DirectX::XMFLOAT3(x1, y1, 0.05f);
    v[3].uv = DirectX::XMFLOAT2(tx1, ty1);
    v[3].color = col;

    verts.push_back(v[0]);
    verts.push_back(v[1]);
    verts.push_back(v[2]);
    verts.push_back(v[2]);
    verts.push_back(v[1]);
    verts.push_back(v[3]);
  };

  // 1. Semi-transparent black banner: (16, 172, 288, 56)
  AddQuad(16, 172, 288, 56, 511.0f, 511.0f, 511.0f, 511.0f,
          {0.05f, 0.05f, 0.08f, 0.82f});

  // 1.1. Subtle silver border lines
  AddQuad(16, 171, 288, 1, 511.0f, 511.0f, 511.0f, 511.0f,
          {0.45f, 0.48f, 0.55f, 0.9f});
  AddQuad(16, 228, 288, 1, 511.0f, 511.0f, 511.0f, 511.0f,
          {0.35f, 0.38f, 0.45f, 0.9f});

  // 2. Text Glyphs
  const std::string &page = m_Pages[m_CurrentPage];
  size_t charsToShow = std::min(m_CharsRevealed, page.size());

  float startX = 26.0f;
  float startY = 179.0f;
  float curX = startX;
  float curY = startY;

  for (size_t i = 0; i < charsToShow; ++i) {
    char c = page[i];
    if (c == '\n') {
      curX = startX;
      curY += 14.0f;
      continue;
    }
    if (c >= 32 && c <= 126) {
      int idx = c - 32;
      int col = idx % 16;
      int row = idx / 16;
      float u0 = col * 8.0f;
      float v0 = 256.0f + row * 8.0f;

      // Drop shadow
      AddQuad(curX + 1.0f, curY + 1.0f, 8.0f, 8.0f, u0, v0, u0 + 8.0f, v0 + 8.0f,
              {0.0f, 0.0f, 0.0f, 0.9f});
      // Glyph (Warm Authentic PS1 Off-White)
      AddQuad(curX, curY, 8.0f, 8.0f, u0, v0, u0 + 8.0f, v0 + 8.0f,
              {0.95f, 0.95f, 0.92f, 1.0f});
    }
    curX += 8.0f;
  }

  // 3. Prompt or Blinking Cursor
  if (m_PageFullyRevealed) {
    if (m_IsPrompt) {
      // Draw Yes / No options on right side
      float pX = 230.0f;
      float pY = 188.0f;

      // Cursor arrow
      float arrY = (m_PromptSelection == 0) ? pY : (pY + 14.0f);
      int arrIdx = '>' - 32;
      float au0 = (arrIdx % 16) * 8.0f;
      float av0 = 256.0f + (arrIdx / 16) * 8.0f;
      AddQuad(pX, arrY, 8.0f, 8.0f, au0, av0, au0 + 8.0f, av0 + 8.0f,
              {1.0f, 0.3f, 0.3f, 1.0f});

      // "Yes"
      const char *yesStr = "Yes";
      for (int k = 0; yesStr[k]; ++k) {
        int g = yesStr[k] - 32;
        float u0 = (g % 16) * 8.0f;
        float v0 = 256.0f + (g / 16) * 8.0f;
        AddQuad(pX + 12.0f + k * 8.0f, pY, 8.0f, 8.0f, u0, v0, u0 + 8.0f,
                v0 + 8.0f, {0.95f, 0.95f, 0.95f, 1.0f});
      }
      // "No"
      const char *noStr = "No";
      for (int k = 0; noStr[k]; ++k) {
        int g = noStr[k] - 32;
        float u0 = (g % 16) * 8.0f;
        float v0 = 256.0f + (g / 16) * 8.0f;
        AddQuad(pX + 12.0f + k * 8.0f, pY + 14.0f, 8.0f, 8.0f, u0, v0,
                u0 + 8.0f, v0 + 8.0f, {0.95f, 0.95f, 0.95f, 1.0f});
      }
    } else {
      // Blinking down triangle/prompt indicator in bottom right corner
      if (std::fmod(m_BlinkTimer, 0.5f) < 0.28f) {
        int g = 'v' - 32;
        float u0 = (g % 16) * 8.0f;
        float v0 = 256.0f + (g / 16) * 8.0f;
        AddQuad(288.0f, 214.0f, 8.0f, 8.0f, u0, v0, u0 + 8.0f, v0 + 8.0f,
                {1.0f, 0.9f, 0.2f, 1.0f});
      }
    }
  }

  m_VertexCount = static_cast<uint32_t>(verts.size());
  std::memcpy(m_VertexBufferMapped, verts.data(),
              verts.size() * sizeof(Render::PS1Vertex));
}

void TextBox::Render(ID3D12GraphicsCommandList *cmdList,
                    Render::PS1Pipeline &pipeline) {
  if (!m_IsActive || !m_VertexBufferMapped) return;
  RebuildVertices();
  if (m_VertexCount == 0) return;

  pipeline.Bind(cmdList);
  Render::SceneConstants constants;
  constants.worldViewProj = DirectX::XMMatrixIdentity();
  constants.prevWorldViewProj = DirectX::XMMatrixIdentity();
  constants.screenResolution =
      DirectX::XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
  constants.blendMode = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f); // Alpha blend
  constants.jitterOffset = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  pipeline.UpdateConstants(cmdList, constants);

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  cmdList->SetGraphicsRootDescriptorTable(1, m_SrvGpuHandle);
  cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(m_VertexCount, 1, 0, 0);
}

} // namespace Game
