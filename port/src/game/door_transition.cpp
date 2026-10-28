#include "door_transition.h"
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

DoorTransition g_DoorTransition;

bool DoorTransition::Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                                const std::wstring &itemM1Dir) {
  m_Device = device;
  m_CmdQueue = cmdQueue;
  m_ItemM1Dir = itemM1Dir;

  // 1. Create SRV Descriptor Heap
  D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
  srvDesc.NumDescriptors = 1;
  srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(m_Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_SrvHeap)))) {
    return false;
  }
  m_SrvGpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();

  // 2. Create Dynamic Vertex Buffer for Door Geometry (Max 4096 vertices)
  constexpr UINT maxVerts = 4096;
  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

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

  // 3. Create Dynamic Vertex Buffer for Fade Quad
  vbDesc.Width = 6 * sizeof(Render::PS1Vertex);
  if (FAILED(m_Device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_FadeVertexBuffer)))) {
    return false;
  }
  m_FadeVertexBuffer->Map(0, nullptr, &m_FadeVertexBufferMapped);
  m_FadeVertexBufferView.BufferLocation = m_FadeVertexBuffer->GetGPUVirtualAddress();
  m_FadeVertexBufferView.StrideInBytes = sizeof(Render::PS1Vertex);
  m_FadeVertexBufferView.SizeInBytes = static_cast<UINT>(vbDesc.Width);

  // Pre-load default Door 0 asset
  LoadDoorAsset(0);

  return true;
}

void DoorTransition::Shutdown() {
  if (m_VertexBuffer) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
    m_VertexBuffer.Reset();
  }
  if (m_FadeVertexBuffer) {
    m_FadeVertexBuffer->Unmap(0, nullptr);
    m_FadeVertexBufferMapped = nullptr;
    m_FadeVertexBuffer.Reset();
  }
  m_DoorTexture.Reset();
  m_SrvHeap.Reset();
  m_IsActive = false;
}

bool DoorTransition::LoadDoorAsset(uint32_t doorSlot) {
  uint32_t slotIdx = doorSlot % 16;
  wchar_t filename[64];
  swprintf_s(filename, L"DOOR%02u.DOR", slotIdx);
  std::wstring fullPath = m_ItemM1Dir + L"/" + filename;

  if (!std::filesystem::exists(fullPath)) {
    fullPath = m_ItemM1Dir + L"/DOOR00.DOR";
    if (!std::filesystem::exists(fullPath)) return false;
  }

  std::ifstream dorFile(fullPath, std::ios::binary | std::ios::ate);
  if (!dorFile.is_open()) return false;
  size_t dorSize = static_cast<size_t>(dorFile.tellg());
  dorFile.seekg(0, std::ios::beg);
  std::vector<uint8_t> dorData(dorSize);
  dorFile.read(reinterpret_cast<char *>(dorData.data()), dorSize);

  if (dorSize < 16) return false;
  uint32_t tmdOff = *reinterpret_cast<const uint32_t *>(dorData.data() + 4);
  uint32_t timOff = *reinterpret_cast<const uint32_t *>(dorData.data() + 8);
  if (tmdOff >= dorSize || timOff >= dorSize) return false;

  // 1. Decode 8-bit TIM Texture at timOff
  if (*reinterpret_cast<const uint32_t *>(dorData.data() + timOff) == 0x10) {
    uint32_t clutBsize = *reinterpret_cast<const uint32_t *>(dorData.data() + timOff + 8);
    const uint16_t *clutWords = reinterpret_cast<const uint16_t *>(dorData.data() + timOff + 20);
    uint32_t numColors = (clutBsize - 12) / 2;
    std::vector<uint32_t> palette(numColors);
    for (uint32_t i = 0; i < numColors; ++i) {
      uint16_t c = clutWords[i];
      uint32_t r = ((c & 0x1F) * 255) / 31;
      uint32_t g = (((c >> 5) & 0x1F) * 255) / 31;
      uint32_t b = (((c >> 10) & 0x1F) * 255) / 31;
      uint32_t a = 255; // Opaque 3D door panel & knob texture
      palette[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    size_t imgHdr = timOff + 8 + clutBsize;
    if (imgHdr + 12 < dorSize) {
      uint16_t iw = *reinterpret_cast<const uint16_t *>(dorData.data() + imgHdr + 8);
      uint16_t ih = *reinterpret_cast<const uint16_t *>(dorData.data() + imgHdr + 10);
      uint32_t pixW = iw * 2; // 8-bit paletted
      const uint8_t *pixData = dorData.data() + imgHdr + 12;

      constexpr uint32_t texW = 256;
      constexpr uint32_t texH = 256;
      std::vector<uint32_t> texPixels(texW * texH, 0);
      for (uint32_t y = 0; y < ih && y < texH; ++y) {
        for (uint32_t x = 0; x < pixW && x < texW; ++x) {
          uint8_t idx = pixData[y * pixW + x];
          if (idx < palette.size()) {
            texPixels[y * texW + x] = palette[idx];
          }
        }
      }

      D3D12_HEAP_PROPERTIES defaultHeapProps = {};
      defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
      D3D12_RESOURCE_DESC texDesc = {};
      texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      texDesc.Width = texW;
      texDesc.Height = texH;
      texDesc.DepthOrArraySize = 1;
      texDesc.MipLevels = 1;
      texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      texDesc.SampleDesc.Count = 1;
      texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

      m_DoorTexture.Reset();
      m_Device->CreateCommittedResource(
          &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
          IID_PPV_ARGS(&m_DoorTexture));

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
      for (uint32_t y = 0; y < texH; ++y) {
        std::memcpy(mappedUpload + layout.Offset + y * layout.Footprint.RowPitch,
                    texPixels.data() + y * texW, texW * 4);
      }
      texUpload->Unmap(0, nullptr);

      Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
      m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       IID_PPV_ARGS(&cmdAlloc));
      Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
      m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(),
                                  nullptr, IID_PPV_ARGS(&cmdList));

      D3D12_TEXTURE_COPY_LOCATION dst = {};
      dst.pResource = m_DoorTexture.Get();
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.SubresourceIndex = 0;

      D3D12_TEXTURE_COPY_LOCATION src = {};
      src.pResource = texUpload.Get();
      src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      src.PlacedFootprint = layout;

      cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = m_DoorTexture.Get();
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

      D3D12_SHADER_RESOURCE_VIEW_DESC srvView = {};
      srvView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srvView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      srvView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvView.Texture2D.MipLevels = 1;
      m_Device->CreateShaderResourceView(
          m_DoorTexture.Get(), &srvView,
          m_SrvHeap->GetCPUDescriptorHandleForHeapStart());
    }
  }

  // 2. Parse PS1 TMD Geometry at tmdOff
  // Obj 0 is the wooden door slab; Obj 1 is the doorknob.
  m_Objects.clear();
  m_BaseVertices.clear();

  if (*reinterpret_cast<const uint32_t *>(dorData.data() + tmdOff) == 0x41) {
    uint32_t numObj = *reinterpret_cast<const uint32_t *>(dorData.data() + tmdOff + 8);
    const uint8_t *objTable = dorData.data() + tmdOff + 12;

    auto appendTmdObject = [&](uint32_t objIdx, bool isKnob, bool isBackKnob,
                               DirectX::XMFLOAT3 attachOff) {
      if (objIdx >= numObj || (objIdx * 28 + 28 > dorSize - (tmdOff + 12))) return;

      const uint8_t *objPtr = objTable + objIdx * 28;
      uint32_t vertTop = *reinterpret_cast<const uint32_t *>(objPtr + 0) + (tmdOff + 12);
      uint32_t nVert = *reinterpret_cast<const uint32_t *>(objPtr + 4);
      uint32_t normTop = *reinterpret_cast<const uint32_t *>(objPtr + 8) + (tmdOff + 12);
      uint32_t nNorm = *reinterpret_cast<const uint32_t *>(objPtr + 12);
      uint32_t primTop = *reinterpret_cast<const uint32_t *>(objPtr + 16) + (tmdOff + 12);
      uint32_t nPrim = *reinterpret_cast<const uint32_t *>(objPtr + 20);

      if (vertTop + nVert * 8 > dorSize || normTop + nNorm * 8 > dorSize) return;

      DoorMeshObject meshObj;
      meshObj.vertexOffset = static_cast<uint32_t>(m_BaseVertices.size());
      meshObj.isKnob = isKnob;
      meshObj.isBackKnob = isBackKnob;
      meshObj.attachOffset = attachOff;

      struct TMDVertex { int16_t x, y, z, pad; };
      const TMDVertex *verts = reinterpret_cast<const TMDVertex *>(dorData.data() + vertTop);
      const TMDVertex *norms = reinterpret_cast<const TMDVertex *>(dorData.data() + normTop);

      auto computeShade = [&](uint16_t normIdx) -> DirectX::XMFLOAT4 {
        if (normIdx >= nNorm) return DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        float nx = norms[normIdx].x / 4096.0f;
        float ny = norms[normIdx].y / 4096.0f;
        float nz = norms[normIdx].z / 4096.0f;
        if (isBackKnob) {
          nx = -nx;
          nz = -nz;
        }
        // Subtle directional light from camera/above (+X, -Y, -Z)
        float dotL = nx * 0.78f - ny * 0.35f - nz * 0.25f;
        float s = std::clamp(0.68f + 0.37f * std::max(0.0f, dotL), 0.55f, 1.08f);
        return DirectX::XMFLOAT4(s, s, s, 1.0f);
      };

      const uint8_t *pPrim = dorData.data() + primTop;
      for (uint32_t p = 0; p < nPrim && (pPrim + 4 <= dorData.data() + dorSize); ++p) {
        uint8_t olen = pPrim[0];
        uint8_t ilen = pPrim[1];
        uint8_t flag = pPrim[2];
        uint8_t mode = pPrim[3];
        (void)olen;
        (void)flag;

        // Authentic PS1 TMD packet size is (ilen + 1) * 4 bytes (1 header word + ilen data words)
        size_t packetSize = static_cast<size_t>(ilen + 1) * 4;
        if (ilen == 0 || pPrim + packetSize > dorData.data() + dorSize) break;

        uint8_t modeBase = mode & 0xFC;

        // 0x34: Gouraud Textured Triangle (TG3, ilen = 6)
        if (modeBase == 0x34 && ilen >= 6) {
          uint8_t u0 = pPrim[4], v0 = pPrim[5];
          uint8_t u1 = pPrim[8], v1 = pPrim[9];
          uint8_t u2 = pPrim[12], v2 = pPrim[13];

          uint16_t n0 = *reinterpret_cast<const uint16_t *>(pPrim + 16);
          uint16_t idx0 = *reinterpret_cast<const uint16_t *>(pPrim + 18);
          uint16_t n1 = *reinterpret_cast<const uint16_t *>(pPrim + 20);
          uint16_t idx1 = *reinterpret_cast<const uint16_t *>(pPrim + 22);
          uint16_t n2 = *reinterpret_cast<const uint16_t *>(pPrim + 24);
          uint16_t idx2 = *reinterpret_cast<const uint16_t *>(pPrim + 26);

          if (idx0 < nVert && idx1 < nVert && idx2 < nVert) {
            uint16_t ids[3] = {idx0, idx1, idx2};
            uint16_t nms[3] = {n0, n1, n2};
            uint8_t us[3] = {u0, u1, u2};
            uint8_t vs[3] = {v0, v1, v2};
            for (int k = 0; k < 3; ++k) {
              Render::PS1Vertex v;
              v.position = DirectX::XMFLOAT3(
                  static_cast<float>(verts[ids[k]].x),
                  static_cast<float>(verts[ids[k]].y),
                  static_cast<float>(verts[ids[k]].z));
              v.uv = DirectX::XMFLOAT2((us[k] + 0.5f) / 256.0f, (vs[k] + 0.5f) / 256.0f);
              v.color = computeShade(nms[k]);
              m_BaseVertices.push_back(v);
            }
          }
        }
        // 0x24: Flat Textured Triangle (TF3, ilen = 5)
        else if (modeBase == 0x24 && ilen >= 5) {
          uint8_t u0 = pPrim[4], v0 = pPrim[5];
          uint8_t u1 = pPrim[8], v1 = pPrim[9];
          uint8_t u2 = pPrim[12], v2 = pPrim[13];
          uint16_t n0 = *reinterpret_cast<const uint16_t *>(pPrim + 16);
          uint16_t idx0 = *reinterpret_cast<const uint16_t *>(pPrim + 18);
          uint16_t idx1 = *reinterpret_cast<const uint16_t *>(pPrim + 20);
          uint16_t idx2 = *reinterpret_cast<const uint16_t *>(pPrim + 22);

          if (idx0 < nVert && idx1 < nVert && idx2 < nVert) {
            uint16_t ids[3] = {idx0, idx1, idx2};
            uint8_t us[3] = {u0, u1, u2};
            uint8_t vs[3] = {v0, v1, v2};
            DirectX::XMFLOAT4 col = computeShade(n0);
            for (int k = 0; k < 3; ++k) {
              Render::PS1Vertex v;
              v.position = DirectX::XMFLOAT3(
                  static_cast<float>(verts[ids[k]].x),
                  static_cast<float>(verts[ids[k]].y),
                  static_cast<float>(verts[ids[k]].z));
              v.uv = DirectX::XMFLOAT2((us[k] + 0.5f) / 256.0f, (vs[k] + 0.5f) / 256.0f);
              v.color = col;
              m_BaseVertices.push_back(v);
            }
          }
        }
        // 0x3C: Gouraud Textured Quad (TG4, ilen = 8)
        else if (modeBase == 0x3C && ilen >= 8) {
          uint8_t u0 = pPrim[4], v0 = pPrim[5];
          uint8_t u1 = pPrim[8], v1 = pPrim[9];
          uint8_t u2 = pPrim[12], v2 = pPrim[13];
          uint8_t u3 = pPrim[16], v3 = pPrim[17];

          uint16_t n0 = *reinterpret_cast<const uint16_t *>(pPrim + 20);
          uint16_t idx0 = *reinterpret_cast<const uint16_t *>(pPrim + 22);
          uint16_t n1 = *reinterpret_cast<const uint16_t *>(pPrim + 24);
          uint16_t idx1 = *reinterpret_cast<const uint16_t *>(pPrim + 26);
          uint16_t n2 = *reinterpret_cast<const uint16_t *>(pPrim + 28);
          uint16_t idx2 = *reinterpret_cast<const uint16_t *>(pPrim + 30);
          uint16_t n3 = *reinterpret_cast<const uint16_t *>(pPrim + 32);
          uint16_t idx3 = *reinterpret_cast<const uint16_t *>(pPrim + 34);

          if (idx0 < nVert && idx1 < nVert && idx2 < nVert && idx3 < nVert) {
            uint16_t ids[4] = {idx0, idx1, idx2, idx3};
            uint16_t nms[4] = {n0, n1, n2, n3};
            uint8_t us[4] = {u0, u1, u2, u3};
            uint8_t vs[4] = {v0, v1, v2, v3};
            Render::PS1Vertex qv[4];
            for (int k = 0; k < 4; ++k) {
              qv[k].position = DirectX::XMFLOAT3(
                  static_cast<float>(verts[ids[k]].x),
                  static_cast<float>(verts[ids[k]].y),
                  static_cast<float>(verts[ids[k]].z));
              qv[k].uv = DirectX::XMFLOAT2((us[k] + 0.5f) / 256.0f, (vs[k] + 0.5f) / 256.0f);
              qv[k].color = computeShade(nms[k]);
            }
            m_BaseVertices.push_back(qv[0]);
            m_BaseVertices.push_back(qv[1]);
            m_BaseVertices.push_back(qv[2]);
            m_BaseVertices.push_back(qv[2]);
            m_BaseVertices.push_back(qv[1]);
            m_BaseVertices.push_back(qv[3]);
          }
        }

        pPrim += packetSize;
      }

      meshObj.vertexCount = static_cast<uint32_t>(m_BaseVertices.size()) - meshObj.vertexOffset;
      m_Objects.push_back(meshObj);
    };

    // Obj 0: Swinging Door Leaf Slab (X in [-145..+143], Y in [-6600..0], Z in [-3599..0])
    appendTmdObject(0, false, false, {0.0f, 0.0f, 0.0f});
    // Obj 1: Front Doorknob attached at (+130, -3240, -3430) on door face
    appendTmdObject(1, true, false, {130.0f, -3240.0f, -3430.0f});
    // Obj 1: Back Doorknob attached at (-130, -3240, -3430) on back of door face
    appendTmdObject(1, true, true, {-130.0f, -3240.0f, -3430.0f});
  }

  return !m_BaseVertices.empty();
}

void DoorTransition::StartTransition(uint32_t doorSlot, uint32_t doorType,
                                     std::function<void()> onComplete) {
  (void)doorType;
  m_CurrentSlot = doorSlot;
  m_Timeline = 0.0f;
  m_DoorAngle = 0.0f;
  m_HandleAngle = 0.0f;
  m_CameraX = 6800.0f;
  m_FadeAlpha = 0.0f;
  m_HandledCutover = false;
  m_OnComplete = onComplete;
  m_IsActive = true;

  LoadDoorAsset(doorSlot);
  g_AudioEngine.PlayDoorOpen();
}

void DoorTransition::Update(float deltaTime, uint32_t buttons) {
  (void)buttons;
  if (!m_IsActive) return;

  m_Timeline += deltaTime;

  // Timeline: 0.0s to 2.45s
  if (m_Timeline < 0.35f) {
    // Phase 1: Doorknob unlatching turn
    float p = m_Timeline / 0.35f;
    float knobCurve = std::sin(p * DirectX::XM_PI);
    m_HandleAngle = knobCurve * (-38.0f * (DirectX::XM_PI / 180.0f));
    m_DoorAngle = 0.0f;
    m_CameraX = 6800.0f;
    m_FadeAlpha = 0.0f;
  } else if (m_Timeline < 2.05f) {
    // Phase 2: Door swings away from camera into the room while camera glides forward
    float p = (m_Timeline - 0.35f) / 1.70f;
    float ease = 0.5f * (1.0f - std::cos(p * DirectX::XM_PI));
    m_HandleAngle = 0.0f;
    m_DoorAngle = ease * (82.0f * (DirectX::XM_PI / 180.0f));
    m_CameraX = 6800.0f - ease * 5400.0f;
    m_FadeAlpha = 0.0f;
  } else if (m_Timeline < 2.45f) {
    // Phase 3: Fade to black inside doorway
    float p = (m_Timeline - 2.05f) / 0.40f;
    m_FadeAlpha = std::min(1.0f, p);
    m_DoorAngle = 82.0f * (DirectX::XM_PI / 180.0f);
    m_CameraX = 1400.0f;
  } else {
    // Phase 4: Room handover!
    m_IsActive = false;
    if (!m_HandledCutover) {
      m_HandledCutover = true;
      if (m_OnComplete) {
        auto cb = m_OnComplete;
        m_OnComplete = nullptr;
        cb();
      }
    }
  }
}

void DoorTransition::RebuildDynamicVertices() {
  if (m_BaseVertices.empty() || !m_VertexBufferMapped) {
    m_VertexCount = 0;
    return;
  }

  m_FrameVertices = m_BaseVertices;
  float cosDoor = std::cos(m_DoorAngle);
  float sinDoor = std::sin(m_DoorAngle);
  float cosKnob = std::cos(m_HandleAngle);
  float sinKnob = std::sin(m_HandleAngle);

  // Transform door slab and attached doorknobs around the door hinge (X=0, Z=0 in local door space,
  // then shifted by Z += 1800 so the 3600-unit wide door is centered on Z=0).
  for (const auto &obj : m_Objects) {
    for (size_t i = 0; i < obj.vertexCount; ++i) {
      size_t idx = obj.vertexOffset + i;
      if (idx >= m_FrameVertices.size()) continue;

      float lx = m_BaseVertices[idx].position.x;
      float ly = m_BaseVertices[idx].position.y;
      float lz = m_BaseVertices[idx].position.z;

      if (obj.isKnob) {
        // Rotate knob around its spindle (local X axis)
        float ky = ly * cosKnob - lz * sinKnob;
        float kz = ly * sinKnob + lz * cosKnob;
        float kx = obj.isBackKnob ? -lx : lx;
        if (obj.isBackKnob) {
          kz = -kz;
        }
        lx = kx + obj.attachOffset.x;
        ly = ky + obj.attachOffset.y;
        lz = kz + obj.attachOffset.z;
      }

      // Swing door around hinge (Y axis) away from camera (into -X)
      float wx = lx * cosDoor + lz * sinDoor;
      float wy = ly;
      float wz = -lx * sinDoor + lz * cosDoor + 1800.0f;

      m_FrameVertices[idx].position = DirectX::XMFLOAT3(wx, wy, wz);
    }
  }

  m_VertexCount = static_cast<uint32_t>(m_FrameVertices.size());
  std::memcpy(m_VertexBufferMapped, m_FrameVertices.data(),
              m_FrameVertices.size() * sizeof(Render::PS1Vertex));

  // Build full-screen fade quad if alpha > 0
  if (m_FadeVertexBufferMapped) {
    Render::PS1Vertex fv[6];
    DirectX::XMFLOAT4 fCol(0.0f, 0.0f, 0.0f, m_FadeAlpha);
    fv[0].position = {-1.0f, 1.0f, 0.0f}; fv[0].color = fCol; fv[0].uv = {0, 0};
    fv[1].position = {1.0f, 1.0f, 0.0f};  fv[1].color = fCol; fv[1].uv = {1, 0};
    fv[2].position = {-1.0f, -1.0f, 0.0f};fv[2].color = fCol; fv[2].uv = {0, 1};
    fv[3].position = {-1.0f, -1.0f, 0.0f};fv[3].color = fCol; fv[3].uv = {0, 1};
    fv[4].position = {1.0f, 1.0f, 0.0f};  fv[4].color = fCol; fv[4].uv = {1, 0};
    fv[5].position = {1.0f, -1.0f, 0.0f}; fv[5].color = fCol; fv[5].uv = {1, 1};
    std::memcpy(m_FadeVertexBufferMapped, fv, sizeof(fv));
  }
}

void DoorTransition::Render(ID3D12GraphicsCommandList *cmdList,
                           Render::PS1Pipeline &pipeline) {
  if (!m_IsActive || !m_VertexBufferMapped) return;
  RebuildDynamicVertices();
  if (m_VertexCount == 0) return;

  pipeline.Bind(cmdList);

  // Authentic DOORxx.DOR camera setup:
  // Eye starts at (+6800, -3400, 0) looking along -X toward the door centered at (0, -3300, 0).
  DirectX::XMVECTOR eye = DirectX::XMVectorSet(m_CameraX, -3400.0f, 0.0f, 1.0f);
  DirectX::XMVECTOR target = DirectX::XMVectorSet(m_CameraX - 1000.0f, -3400.0f, 0.0f, 1.0f);
  DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f); // PS1 -Y is Up

  DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
  DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(
      DirectX::XMConvertToRadians(58.0f), 4.0f / 3.0f, 50.0f, 25000.0f);
  // Match PS1 right-handed horizontal axis when using camUp = (0, -1, 0)
  proj.r[0].m128_f32[0] = -proj.r[0].m128_f32[0];
  DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);

  Render::SceneConstants constants;
  constants.worldViewProj = viewProj;
  constants.prevWorldViewProj = viewProj;
  constants.screenResolution =
      DirectX::XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = DirectX::XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f); // 3D Gouraud + Textured
  constants.blendMode = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  constants.jitterOffset = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  pipeline.UpdateConstants(cmdList, constants);

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  cmdList->SetGraphicsRootDescriptorTable(1, m_SrvGpuHandle);
  cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(m_VertexCount, 1, 0, 0);

  // Draw black fade if alpha > 0
  if (m_FadeAlpha > 0.0f && m_FadeVertexBufferMapped) {
    constants.worldViewProj = DirectX::XMMatrixIdentity();
    constants.prevWorldViewProj = DirectX::XMMatrixIdentity();
    constants.renderFlags = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    constants.blendMode = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f); // Alpha blend
    pipeline.UpdateConstants(cmdList, constants);

    cmdList->IASetVertexBuffers(0, 1, &m_FadeVertexBufferView);
    cmdList->DrawInstanced(6, 1, 0, 0);
  }
}

} // namespace Game
