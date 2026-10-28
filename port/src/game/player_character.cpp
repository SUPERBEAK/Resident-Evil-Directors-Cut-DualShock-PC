#include "player_character.h"
#include "audio_engine.h"
#include "psx_pad.h"
#include "psx_tim.h"
#include "room_manager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <windows.h>

namespace Game {

bool PlayerCharacter::Initialize(ID3D12Device *device,
                                 ID3D12CommandQueue *cmdQueue,
                                 const std::wstring &emdPath) {
  m_Device = device;
  m_CmdQueue = cmdQueue;

  // 1. Read EMD File from disk
  std::ifstream file(emdPath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::fprintf(stderr, "[PlayerCharacter] Could not open EMD file: %ls\n",
                 emdPath.c_str());
    return false;
  }

  size_t size = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  file.read(reinterpret_cast<char *>(buffer.data()), size);

  if (size < 0x9abc + 128) {
    std::fprintf(stderr, "[PlayerCharacter] EMD file too small: %zu bytes\n",
                 size);
    return false;
  }

  // 2. Decode Player TIM Texture (offset 0x9abc for CHAR10.EMD)
  PSX::TimImage tim;
  const uint8_t *timPtr = buffer.data() + 0x9abc;
  size_t timSize = size - 0x9abc;
  if (!PSX::TimDecoder::LoadFromMemory(timPtr, timSize, tim)) {
    std::fprintf(stderr,
                 "[PlayerCharacter] Failed to decode TIM texture from EMD\n");
    return false;
  }

  // 3. Create GPU Texture2D & Upload Heap
  D3D12_HEAP_PROPERTIES defaultHeapProps = {};
  defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = tim.width;
  texDesc.Height = tim.height;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

  if (FAILED(device->CreateCommittedResource(
          &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_Texture)))) {
    return false;
  }

  UINT64 uploadBufferSize = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
  UINT numRows = 0;
  UINT64 rowSizeInBytes = 0;
  UINT64 totalBytes = 0;
  device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, &numRows,
                                &rowSizeInBytes, &totalBytes);
  uploadBufferSize = totalBytes;

  ComPtr<ID3D12Resource> textureUpload;
  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = uploadBufferSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  if (FAILED(device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&textureUpload)))) {
    return false;
  }

  uint8_t *pUploadData = nullptr;
  D3D12_RANGE readRange = {0, 0};
  textureUpload->Map(0, &readRange, reinterpret_cast<void **>(&pUploadData));
  for (UINT y = 0; y < tim.height; ++y) {
    memcpy(pUploadData + layout.Offset + y * layout.Footprint.RowPitch,
           tim.rgbaPixels.data() + y * tim.width, tim.width * sizeof(uint32_t));
  }
  textureUpload->Unmap(0, nullptr);

  // Copy to default heap via transient command list
  ComPtr<ID3D12CommandAllocator> cmdAlloc;
  ComPtr<ID3D12GraphicsCommandList> cmdList;
  device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 IID_PPV_ARGS(&cmdAlloc));
  device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(),
                            nullptr, IID_PPV_ARGS(&cmdList));

  D3D12_TEXTURE_COPY_LOCATION dst = {};
  dst.pResource = m_Texture.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = 0;

  D3D12_TEXTURE_COPY_LOCATION src = {};
  src.pResource = textureUpload.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = layout;

  cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_Texture.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmdList->ResourceBarrier(1, &barrier);

  cmdList->Close();
  ID3D12CommandList *ppCmdLists[] = {cmdList.Get()};
  cmdQueue->ExecuteCommandLists(1, ppCmdLists);

  ComPtr<ID3D12Fence> fence;
  device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  cmdQueue->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, hEvent);
  WaitForSingleObjectEx(hEvent, INFINITE, FALSE);
  CloseHandle(hEvent);

  // 4. Create SRV Descriptor Heap
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.NumDescriptors = 2;
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc,
                                          IID_PPV_ARGS(&m_SrvHeap)))) {
    return false;
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;

  m_SrvGpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
  device->CreateShaderResourceView(
      m_Texture.Get(), &srvDesc,
      m_SrvHeap->GetCPUDescriptorHandleForHeapStart());

  // 5. Parse 16 Joint TMD Meshes from offset 0x3584 with 1.0f scale
  const uint8_t *tmdPtr = buffer.data() + 0x3584;
  size_t tmdSize = size - 0x3584;
  if (!PSX::TmdLoader::LoadFromMemory(tmdPtr, tmdSize, m_JointMeshes, 1.0f)) {
    std::fprintf(stderr,
                 "[PlayerCharacter] Failed to parse player TMD joint meshes\n");
    return false;
  }
  std::printf("[PlayerCharacter] Loaded %zu skeletal joint meshes\n",
              m_JointMeshes.size());

  // 6. Parse Armature & Joint Relative Offsets from EMR header
  uint16_t armOff = *reinterpret_cast<const uint16_t *>(buffer.data());
  uint16_t framesOff = *reinterpret_cast<const uint16_t *>(buffer.data() + 2);
  uint16_t frameSize = *reinterpret_cast<const uint16_t *>(buffer.data() + 6);

  m_Skeleton.resize(15);
  const int16_t *baseVecs =
      reinterpret_cast<const int16_t *>(buffer.data() + 8);
  for (int j = 0; j < 15; ++j) {
    m_Skeleton[j].baseOffset =
        XMFLOAT3(static_cast<float>(baseVecs[j * 3 + 0]),
                 static_cast<float>(baseVecs[j * 3 + 1]),
                 static_cast<float>(baseVecs[j * 3 + 2]));
    m_Skeleton[j].parentIndex = -1;
  }

  // Armature table: each joint j has a 4-byte record: { u8 child_count, u8 pad,
  // u16 children_offset }
  const uint8_t *armPtr = buffer.data() + armOff;
  for (int j = 0; j < 15; ++j) {
    uint8_t childCount = armPtr[j * 4 + 0];
    uint16_t childrenOff =
        *reinterpret_cast<const uint16_t *>(armPtr + j * 4 + 2);
    if (childCount > 0 && childrenOff < 0x100) {
      const uint8_t *children = armPtr + childrenOff;
      for (uint8_t c = 0; c < childCount; ++c) {
        uint8_t childIdx = children[c];
        if (childIdx < 15) {
          m_Skeleton[childIdx].parentIndex = j;
        }
      }
    }
  }

  // 7. Parse Animation Frames (0xb0..0x32ac)
  size_t animEnd = 0x32ac;
  if (animEnd > size)
    animEnd = size;
  uint32_t numFrames = static_cast<uint32_t>((animEnd - framesOff) / frameSize);
  m_AnimFrames.resize(numFrames);

  for (uint32_t f = 0; f < numFrames; ++f) {
    const uint8_t *fPtr = buffer.data() + framesOff + f * frameSize;
    const int16_t *root = reinterpret_cast<const int16_t *>(fPtr);
    m_AnimFrames[f].rootOffset =
        XMFLOAT3(static_cast<float>(root[0]), static_cast<float>(root[1]),
                 static_cast<float>(root[2]));

    const int16_t *rootRot = reinterpret_cast<const int16_t *>(fPtr + 6);
    constexpr float toRad = XM_2PI / 4096.0f;
    m_AnimFrames[f].rootRotation =
        XMFLOAT3(static_cast<float>(rootRot[0]) * toRad,
                 static_cast<float>(rootRot[1]) * toRad,
                 static_cast<float>(rootRot[2]) * toRad);

    m_AnimFrames[f].jointRotations.resize(15);
    for (int j = 0; j < 15; ++j) {
      // Bytes 0..5: root translation, Bytes 6..11: root rotation,
      // Bytes 12..101: 15 joint rotations
      const int16_t *rot = reinterpret_cast<const int16_t *>(fPtr + 12 + j * 6);
      m_AnimFrames[f].jointRotations[j] =
          XMFLOAT3(static_cast<float>(rot[0]) * toRad,
                   static_cast<float>(rot[1]) * toRad,
                   static_cast<float>(rot[2]) * toRad);
    }
  }
  std::printf("[PlayerCharacter] Parsed %u animation frames from EMR\n",
              numFrames);

  // 7b. Parse Unarmed Weapon Locomotion Frames from PLAYERS/W00.EMW (if present)
  {
    std::wstring w00Path = emdPath;
    size_t enemyPos = w00Path.find(L"ENEMY");
    if (enemyPos != std::wstring::npos) {
      w00Path = w00Path.substr(0, enemyPos) + L"PLAYERS/W00.EMW";
    } else {
      w00Path = L"extracted/RE1-Dualshock/PLAYERS/W00.EMW";
    }
    std::ifstream w00File(w00Path, std::ios::binary | std::ios::ate);
    if (!w00File.is_open()) {
      w00File.open(L"../../extracted/RE1-Dualshock/PLAYERS/W00.EMW",
                   std::ios::binary | std::ios::ate);
    }
    if (w00File.is_open()) {
      size_t w00Size = static_cast<size_t>(w00File.tellg());
      w00File.seekg(0, std::ios::beg);
      std::vector<uint8_t> w00Buf(w00Size);
      w00File.read(reinterpret_cast<char *>(w00Buf.data()), w00Size);
      if (w00Size >= 0xB0 + 104) {
        uint16_t wFramesOff = *reinterpret_cast<const uint16_t *>(w00Buf.data() + 2);
        uint16_t wFrameSize = *reinterpret_cast<const uint16_t *>(w00Buf.data() + 6);
        size_t wAnimEnd = 0x2CE0; // EDD offset in W00.EMW (108 frames)
        if (wAnimEnd > w00Size)
          wAnimEnd = w00Size;
        uint32_t wNumFrames = static_cast<uint32_t>((wAnimEnd - wFramesOff) / wFrameSize);
        m_UnarmedWeaponAnimFrames.resize(wNumFrames);
        constexpr float toRad = XM_2PI / 4096.0f;
        for (uint32_t f = 0; f < wNumFrames; ++f) {
          const uint8_t *fPtr = w00Buf.data() + wFramesOff + f * wFrameSize;
          const int16_t *root = reinterpret_cast<const int16_t *>(fPtr);
          m_UnarmedWeaponAnimFrames[f].rootOffset =
              XMFLOAT3(static_cast<float>(root[0]), static_cast<float>(root[1]),
                       static_cast<float>(root[2]));
          const int16_t *rootRot = reinterpret_cast<const int16_t *>(fPtr + 6);
          m_UnarmedWeaponAnimFrames[f].rootRotation =
              XMFLOAT3(static_cast<float>(rootRot[0]) * toRad,
                       static_cast<float>(rootRot[1]) * toRad,
                       static_cast<float>(rootRot[2]) * toRad);
          m_UnarmedWeaponAnimFrames[f].jointRotations.resize(15);
          for (int j = 0; j < 15; ++j) {
            const int16_t *rot = reinterpret_cast<const int16_t *>(fPtr + 12 + j * 6);
            m_UnarmedWeaponAnimFrames[f].jointRotations[j] =
                XMFLOAT3(static_cast<float>(rot[0]) * toRad,
                         static_cast<float>(rot[1]) * toRad,
                         static_cast<float>(rot[2]) * toRad);
          }
        }
        std::printf("[PlayerCharacter] Parsed %u unarmed locomotion frames from W00.EMW\n",
                    wNumFrames);
      }
    }
  }

  // 8. Count total render vertices & create upload vertex buffer
  size_t totalVertices = 0;
  for (const auto &mesh : m_JointMeshes) {
    totalVertices += mesh.vertices.size();
  }
  m_CurrentPoseVertices.resize(totalVertices);

  UINT vertexBufferSize =
      static_cast<UINT>(totalVertices * sizeof(Render::PS1Vertex));
  bufferDesc.Width = vertexBufferSize;

  if (FAILED(device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_VertexBuffer)))) {
    return false;
  }

  m_VertexBuffer->Map(0, &readRange,
                      reinterpret_cast<void **>(&m_VertexBufferMapped));

  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Render::PS1Vertex);
  m_VertexBufferView.SizeInBytes = vertexBufferSize;

  // 9. Build Initial Rest Pose (Frame 0 - Authentic Standing Pose)
  m_CurrentAnimFrame = 0.0f;
  BuildSkeletalPoseInterpolated(0.0f, 0, 0);
  UploadCurrentPose();

  m_IsLoaded = true;
  std::printf("[PlayerCharacter] Player character initialized successfully "
              "(%zu total vertices)\n",
              totalVertices);
  return true;
}

void PlayerCharacter::Shutdown() {
  if (m_VertexBuffer && m_VertexBufferMapped) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
  }
  m_IsLoaded = false;
}

bool PlayerCharacter::LoadEquippedWeapon(const std::wstring &emwPath,
                                         const std::wstring &tmdPath) {
  std::ifstream file(emwPath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::fprintf(stderr, "[PlayerCharacter] Could not open EMW file: %ls\n",
                 emwPath.c_str());
    return false;
  }
  size_t size = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  file.read(reinterpret_cast<char *>(buffer.data()), size);

  if (size < 0xB0 + 104) {
    return false;
  }

  uint16_t framesOff = *reinterpret_cast<const uint16_t *>(buffer.data() + 2);
  uint16_t frameSize = *reinterpret_cast<const uint16_t *>(buffer.data() + 6);
  size_t animEnd = 21288; // EDD starts at offset 21288 in W02.EMW
  if (animEnd > size)
    animEnd = size;

  uint32_t numFrames = static_cast<uint32_t>((animEnd - framesOff) / frameSize);
  m_WeaponAnimFrames.resize(numFrames);

  constexpr float toRad = XM_2PI / 4096.0f;
  for (uint32_t f = 0; f < numFrames; ++f) {
    const uint8_t *fPtr = buffer.data() + framesOff + f * frameSize;
    const int16_t *root = reinterpret_cast<const int16_t *>(fPtr);
    m_WeaponAnimFrames[f].rootOffset =
        XMFLOAT3(static_cast<float>(root[0]), static_cast<float>(root[1]),
                 static_cast<float>(root[2]));

    const int16_t *rootRot = reinterpret_cast<const int16_t *>(fPtr + 6);
    m_WeaponAnimFrames[f].rootRotation =
        XMFLOAT3(static_cast<float>(rootRot[0]) * toRad,
                 static_cast<float>(rootRot[1]) * toRad,
                 static_cast<float>(rootRot[2]) * toRad);

    m_WeaponAnimFrames[f].jointRotations.resize(15);
    for (int j = 0; j < 15; ++j) {
      const int16_t *rot = reinterpret_cast<const int16_t *>(fPtr + 12 + j * 6);
      m_WeaponAnimFrames[f].jointRotations[j] =
          XMFLOAT3(static_cast<float>(rot[0]) * toRad,
                   static_cast<float>(rot[1]) * toRad,
                   static_cast<float>(rot[2]) * toRad);
    }
  }

  // 2. Load Weapon TMD Mesh
  if (!PSX::TmdLoader::LoadFromFile(tmdPath, m_WeaponMeshes, 1.0f)) {
    if (size >= 22260 + 12) {
      PSX::TmdLoader::LoadFromMemory(buffer.data() + 22260, size - 22260,
                                     m_WeaponMeshes, 1.0f);
    }
  }

  // 3. Reallocate Vertex Buffer for joint meshes + weapon mesh + drop shadow
  size_t totalVertices = 0;
  for (const auto &mesh : m_JointMeshes) {
    totalVertices += mesh.vertices.size();
  }
  for (const auto &wmesh : m_WeaponMeshes) {
    totalVertices += wmesh.vertices.size();
  }

  m_CurrentPoseVertices.resize(totalVertices);

  if (m_VertexBuffer && m_VertexBufferMapped) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
    m_VertexBuffer.Reset();
  }

  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = totalVertices * sizeof(Render::PS1Vertex);
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  D3D12_RANGE readRange = {0, 0};
  if (FAILED(m_Device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_VertexBuffer)))) {
    return false;
  }

  m_VertexBuffer->Map(0, &readRange,
                      reinterpret_cast<void **>(&m_VertexBufferMapped));
  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Render::PS1Vertex);
  m_VertexBufferView.SizeInBytes = static_cast<UINT>(bufferDesc.Width);

  if (m_UnarmedWeaponAnimFrames.empty() && m_WeaponAnimFrames.size() >= 108) {
    m_UnarmedWeaponAnimFrames.assign(m_WeaponAnimFrames.begin(),
                                     m_WeaponAnimFrames.begin() + 108);
  }

  m_WeaponEquipped = true;
  m_UseBaseEmdAnim = false;
  m_LocoMode = LocomotionMode::Idle;
  m_AmmoCount = 15;
  m_MaxAmmo = 15;
  m_WeaponAnimState = WeaponAnimState::None;
  m_AimPitch = AimPitch::Straight;

  m_CurrentAnimFrame = 19.0f;
  BuildSkeletalPoseInterpolated(19.0f, 19, 34);
  UploadCurrentPose();

  std::printf("[PlayerCharacter] Equipped Handgun (W02.EMW): %u anim frames, "
              "%zu weapon vertices\n",
              numFrames,
              m_WeaponMeshes.empty() ? 0 : m_WeaponMeshes[0].vertices.size());
  return true;
}

void PlayerCharacter::FireWeapon() {
  if (!m_WeaponEquipped || m_WeaponAnimState == WeaponAnimState::Firing ||
      m_WeaponAnimState == WeaponAnimState::Reloading)
    return;

  if (m_AmmoCount > 0) {
    --m_AmmoCount;
    m_WeaponAnimState = WeaponAnimState::Firing;
    switch (m_AimPitch) {
    case AimPitch::Up:
      m_CurrentAnimFrame = 137.0f; // Clip 9 (137..151)
      break;
    case AimPitch::Down:
      m_CurrentAnimFrame = 158.0f; // Clip 12 (158..172)
      break;
    case AimPitch::Straight:
    default:
      m_CurrentAnimFrame = 116.0f; // Clip 6 (116..130)
      break;
    }

    g_AudioEngine.PlayGunshot(1.0f);
    SetVibration(180, 240); // DualShock small + large motor rumble
    m_RumbleTimer = 0.20f;  // ~12 frames duration

    const char *pitchName =
        (m_AimPitch == AimPitch::Up)
            ? "UP"
            : (m_AimPitch == AimPitch::Down ? "DOWN" : "STRAIGHT");
    std::printf(
        "[Combat] Chris FIRED Beretta M92FS (%s)! Remaining Ammo: %d / %d\n",
        pitchName, m_AmmoCount, m_MaxAmmo);
  } else {
    g_AudioEngine.PlayDryFire(0.85f);
    std::printf("[Combat] *Click* Handgun empty! Press R to reload.\n");
  }
}

void PlayerCharacter::Reload() {
  if (!m_WeaponEquipped || m_WeaponAnimState == WeaponAnimState::Reloading)
    return;

  m_WeaponAnimState = WeaponAnimState::Reloading;
  m_CurrentAnimFrame = 173.0f; // Clip 14 (173..202)
  g_AudioEngine.PlayReload(0.9f);
  std::printf("[Combat] Reloading Beretta M92FS handgun...\n");
}

void PlayerCharacter::TriggerPoseBlend() {
  if (!m_LastJointRotations.empty()) {
    m_BlendSrcRootOffset = m_LastRootOffset;
    m_BlendSrcJointRotations = m_LastJointRotations;
    m_PoseBlendAlpha = 0.75f; // Authentic 3-tick (6 frames @ 60fps) LoadAverageShort12 cross-fade
  }
}

void PlayerCharacter::BuildSkeletalPose(uint32_t frameIdx) {
  BuildSkeletalPoseInterpolated(static_cast<float>(frameIdx), 0, 0);
}

static inline float LerpAngleShortest(float a, float b, float t) {
  float diff = b - a;
  while (diff > XM_PI)
    diff -= XM_2PI;
  while (diff < -XM_PI)
    diff += XM_2PI;
  return a + diff * t;
}

void PlayerCharacter::BuildSkeletalPoseInterpolated(float currentFrame,
                                                    uint32_t clipStart,
                                                    uint32_t clipEnd) {
  const std::vector<PlayerAnimFrame> *pFrames = &m_AnimFrames;
  if (!m_UseBaseEmdAnim) {
    if (m_WeaponEquipped && !m_WeaponAnimFrames.empty()) {
      pFrames = &m_WeaponAnimFrames;
    } else if (!m_UnarmedWeaponAnimFrames.empty()) {
      pFrames = &m_UnarmedWeaponAnimFrames;
    } else if (!m_WeaponAnimFrames.empty()) {
      pFrames = &m_WeaponAnimFrames;
    }
  }
  const auto &animFrames = *pFrames;
  if (animFrames.empty() || m_JointMeshes.empty())
    return;

  uint32_t maxFrame = static_cast<uint32_t>(animFrames.size() - 1);
  if (clipStart > maxFrame)
    clipStart = maxFrame;
  if (clipEnd > maxFrame)
    clipEnd = maxFrame;

  uint32_t f0 = static_cast<uint32_t>(currentFrame);
  if (f0 < clipStart || f0 > clipEnd)
    f0 = clipStart;

  uint32_t f1 = f0 + 1;
  if (f1 > clipEnd)
    f1 = clipStart;

  float t = currentFrame - static_cast<float>(f0);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  const auto &frame0 = animFrames[f0];
  const auto &frame1 = animFrames[f1];

  // Lerp root offset
  XMFLOAT3 rootOffset(
      frame0.rootOffset.x * (1.0f - t) + frame1.rootOffset.x * t,
      frame0.rootOffset.y * (1.0f - t) + frame1.rootOffset.y * t,
      frame0.rootOffset.z * (1.0f - t) + frame1.rootOffset.z * t);

  // Lerp joint rotations using shortest-arc interpolation
  size_t jointCount = m_Skeleton.size();
  std::vector<XMFLOAT3> interpRotations(jointCount, XMFLOAT3(0.0f, 0.0f, 0.0f));
  for (size_t j = 0; j < jointCount; ++j) {
    if (j < frame0.jointRotations.size() && j < frame1.jointRotations.size()) {
      interpRotations[j] = XMFLOAT3(
          LerpAngleShortest(frame0.jointRotations[j].x, frame1.jointRotations[j].x, t),
          LerpAngleShortest(frame0.jointRotations[j].y, frame1.jointRotations[j].y, t),
          LerpAngleShortest(frame0.jointRotations[j].z, frame1.jointRotations[j].z, t));
    }
  }

  // Authentic PS1 Head Joint (Joint 1) default rot_delta_z = +16 (player.c:121, model.c:129)
  if (interpRotations.size() > 1) {
    constexpr float toRad = XM_2PI / 4096.0f;
    interpRotations[1].z += 16.0f * toRad;
  }

  // Authentic PS1 3-tick (6 frames @ 60fps) LoadAverageShort12 pose cross-fade (model.c:92-119)
  if (m_PoseBlendAlpha > 0.0f && m_BlendSrcJointRotations.size() == jointCount) {
    float a = m_PoseBlendAlpha;
    rootOffset.x = m_BlendSrcRootOffset.x * a + rootOffset.x * (1.0f - a);
    rootOffset.y = m_BlendSrcRootOffset.y * a + rootOffset.y * (1.0f - a);
    rootOffset.z = m_BlendSrcRootOffset.z * a + rootOffset.z * (1.0f - a);
    for (size_t j = 0; j < jointCount; ++j) {
      interpRotations[j].x = LerpAngleShortest(interpRotations[j].x, m_BlendSrcJointRotations[j].x, a);
      interpRotations[j].y = LerpAngleShortest(interpRotations[j].y, m_BlendSrcJointRotations[j].y, a);
      interpRotations[j].z = LerpAngleShortest(interpRotations[j].z, m_BlendSrcJointRotations[j].z, a);
    }
    m_PoseBlendAlpha -= (1.0f / 6.0f);
    if (m_PoseBlendAlpha < 0.0f)
      m_PoseBlendAlpha = 0.0f;
  }

  m_LastRootOffset = rootOffset;
  m_LastJointRotations = interpRotations;

  std::vector<XMMATRIX> jointMatrices(m_JointMeshes.size(), XMMatrixIdentity());

  auto ComputeJointMatrix = [&](auto &self, int j) -> XMMATRIX {
    if (j < 0 || j >= static_cast<int>(m_Skeleton.size()))
      return XMMatrixIdentity();

    // Authentic PsyQ RotMatrix (R_x * R_y * R_z on column vectors = rz * ry * rx on row vectors)
    // Note: EMR bytes 6..11 (speed_x, speed_y, speed_z) are skipped via data += 4 in joint_move (model.c:87, 124)
    XMMATRIX rot = XMMatrixIdentity();
    if (j < static_cast<int>(interpRotations.size())) {
      const auto &r = interpRotations[j];
      XMMATRIX rx = XMMatrixRotationX(r.x);
      XMMATRIX ry = XMMatrixRotationY(r.y);
      XMMATRIX rz = XMMatrixRotationZ(r.z);
      rot = rz * ry * rx;
    }

    XMMATRIX trans;
    if (j == 0) {
      trans = XMMatrixTranslation(rootOffset.x, rootOffset.y, rootOffset.z);
    } else {
      trans = XMMatrixTranslation(m_Skeleton[j].baseOffset.x,
                                  m_Skeleton[j].baseOffset.y,
                                  m_Skeleton[j].baseOffset.z);
    }

    XMMATRIX local = rot * trans;

    int parent = m_Skeleton[j].parentIndex;
    if (parent >= 0 && parent < static_cast<int>(m_Skeleton.size())) {
      return local * self(self, parent);
    }
    return local;
  };

  for (size_t j = 0; j < m_JointMeshes.size() && j < m_Skeleton.size(); ++j) {
    jointMatrices[j] =
        ComputeJointMatrix(ComputeJointMatrix, static_cast<int>(j));
  }
  if (m_JointMeshes.size() > 15 && jointMatrices.size() > 0) {
    jointMatrices[15] = jointMatrices[0]; // Parent holster/sheath to Joint 0 (Pelvis)
  }

  size_t outIdx = 0;
  for (size_t j = 0; j < m_JointMeshes.size(); ++j) {
    const auto &mesh = m_JointMeshes[j];
    XMMATRIX mtx =
        (j < jointMatrices.size()) ? jointMatrices[j] : XMMatrixIdentity();

    for (const auto &v : mesh.vertices) {
      XMVECTOR pos =
          XMVectorSet(v.position.x, v.position.y, v.position.z, 1.0f);
      pos = XMVector3Transform(pos, mtx);

      Render::PS1Vertex transformed = v;
      transformed.position.x = XMVectorGetX(pos);
      transformed.position.y = XMVectorGetY(pos);
      transformed.position.z = XMVectorGetZ(pos);

      if (outIdx < m_CurrentPoseVertices.size()) {
        m_CurrentPoseVertices[outIdx++] = transformed;
      }
    }
  }

  // Append Weapon Mesh (parented to Joint 11 - Chris's Right Hand)
  if (m_WeaponEquipped && !m_WeaponMeshes.empty() && 11 < jointMatrices.size()) {
    XMMATRIX gunMtx = jointMatrices[11];
    for (const auto &v : m_WeaponMeshes[0].vertices) {
      XMVECTOR pos =
          XMVectorSet(v.position.x, v.position.y, v.position.z, 1.0f);
      pos = XMVector3Transform(pos, gunMtx);

      Render::PS1Vertex transformed = v;
      transformed.position.x = XMVectorGetX(pos);
      transformed.position.y = XMVectorGetY(pos);
      transformed.position.z = XMVectorGetZ(pos);

      if (outIdx < m_CurrentPoseVertices.size()) {
        m_CurrentPoseVertices[outIdx++] = transformed;
      }
    }
  }


}

void PlayerCharacter::UploadCurrentPose() {
  if (m_VertexBufferMapped && !m_CurrentPoseVertices.empty()) {
    std::memcpy(m_VertexBufferMapped, m_CurrentPoseVertices.data(),
                m_CurrentPoseVertices.size() * sizeof(Render::PS1Vertex));
  }
}

void PlayerCharacter::Update(float deltaTime, uint32_t padButtons,
                             const RoomManager *roomMgr) {
  if (!m_IsLoaded)
    return;

  // Handle DualShock Vibration decay
  if (m_RumbleTimer > 0.0f) {
    m_RumbleTimer -= deltaTime;
    if (m_RumbleTimer <= 0.0f) {
      SetVibration(0, 0);
    }
  }

  // Classic Tank Controls - Keyboard + Gamepad dual mapping
  bool moveFwd = (GetAsyncKeyState('W') & 0x8000) != 0 ||
                 (GetAsyncKeyState(VK_UP) & 0x8000) != 0 ||
                 (padButtons & (1 << 4)) != 0;
  bool moveBack = (GetAsyncKeyState('S') & 0x8000) != 0 ||
                  (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 ||
                  (padButtons & (1 << 5)) != 0;
  bool turnLeft = (GetAsyncKeyState('A') & 0x8000) != 0 ||
                  (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0 ||
                  (padButtons & (1 << 6)) != 0;
  bool turnRight = (GetAsyncKeyState('D') & 0x8000) != 0 ||
                   (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0 ||
                   (padButtons & (1 << 7)) != 0;
  bool isRunning = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 ||
                   (GetAsyncKeyState('X') & 0x8000) != 0 ||
                   (padButtons & (1 << 15)) != 0;

  bool aimHeld = (GetAsyncKeyState('F') & 0x8000) != 0 ||
                 (GetAsyncKeyState('V') & 0x8000) != 0 ||
                 (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ||
                 (padButtons & (1 << 11)) != 0;

  // Fire action: pressing Enter, Left Mouse, or Gamepad Square/Cross while
  // aiming
  static bool s_PrevFireDown = false;
  bool fireRaw = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
                 (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
                 (GetAsyncKeyState('Z') & 0x8000) != 0 ||
                 (padButtons & (1 << 2)) != 0;
  bool firePressed = (fireRaw && !s_PrevFireDown);
  s_PrevFireDown = fireRaw;

  // Reload action: 'R' key or Gamepad Y
  static bool s_PrevReloadDown = false;
  bool reloadRaw =
      (GetAsyncKeyState('R') & 0x8000) != 0 || (padButtons & (1 << 12)) != 0;
  bool reloadPressed = (reloadRaw && !s_PrevReloadDown);
  s_PrevReloadDown = reloadRaw;

  if (reloadPressed && m_WeaponEquipped && m_AmmoCount < m_MaxAmmo &&
      m_WeaponAnimState != WeaponAnimState::Reloading) {
    Reload();
  }

  uint32_t clipStart = 0;
  uint32_t clipEnd = 0;
  float prevFrame = m_CurrentAnimFrame;

  if (m_WeaponEquipped &&
      (aimHeld || m_WeaponAnimState == WeaponAnimState::Firing ||
       m_WeaponAnimState == WeaponAnimState::Reloading)) {
    // -------------------------------------------------------------
    // WEAPON AIMING & FIRING COMBAT STATE MACHINE
    // -------------------------------------------------------------
    m_IsMoving = false;
    m_IsRunning = false;
    m_UseBaseEmdAnim = false;
    m_LocoMode = LocomotionMode::Idle;

    // Turning while aiming (rotates stance slowly)
    constexpr float aimTurnSpeed = 0.035f;
    if (turnLeft)
      m_FacingAngle -= aimTurnSpeed;
    if (turnRight)
      m_FacingAngle += aimTurnSpeed;

    if (m_WeaponAnimState == WeaponAnimState::None) {
      // Begin Gun Raise: Clip 5 (frames 108..115)
      m_WeaponAnimState = WeaponAnimState::Raise;
      m_AimPitch = AimPitch::Straight;
      m_CurrentAnimFrame = 108.0f;
    }

    if (m_WeaponAnimState == WeaponAnimState::Raise) {
      clipStart = 108;
      clipEnd = 115;
      m_CurrentAnimFrame += 0.55f;
      if (m_CurrentAnimFrame >= 115.0f) {
        m_WeaponAnimState = WeaponAnimState::AimHold;
        m_CurrentAnimFrame = 115.0f;
      }
    } else if (m_WeaponAnimState == WeaponAnimState::AimHold) {
      // Determine vertical aiming target
      if (moveFwd) {
        m_TargetAimPitch = AimPitch::Up;
      } else if (moveBack) {
        m_TargetAimPitch = AimPitch::Down;
      } else {
        m_TargetAimPitch = AimPitch::Straight;
      }

      // Check transitions
      if (m_TargetAimPitch == AimPitch::Up && m_AimPitch != AimPitch::Up) {
        m_WeaponAnimState = WeaponAnimState::AimTransition;
        m_CurrentAnimFrame = 131.0f; // Clip 8: Up transition (131..136)
      } else if (m_TargetAimPitch == AimPitch::Down &&
                 m_AimPitch != AimPitch::Down) {
        m_WeaponAnimState = WeaponAnimState::AimTransition;
        m_CurrentAnimFrame = 152.0f; // Clip 11: Down transition (152..157)
      } else if (m_TargetAimPitch == AimPitch::Straight &&
                 m_AimPitch != AimPitch::Straight) {
        m_AimPitch = AimPitch::Straight;
        m_CurrentAnimFrame = 115.0f;
      }

      // Hold target frame
      if (m_AimPitch == AimPitch::Up) {
        clipStart = 137;
        clipEnd = 137;
        m_CurrentAnimFrame = 137.0f; // Clip 10: Hold Up
      } else if (m_AimPitch == AimPitch::Down) {
        clipStart = 158;
        clipEnd = 158;
        m_CurrentAnimFrame = 158.0f; // Clip 13: Hold Down
      } else {
        clipStart = 115;
        clipEnd = 115;
        m_CurrentAnimFrame = 115.0f; // Clip 7: Hold Straight
      }

      if (firePressed) {
        FireWeapon();
      }
    } else if (m_WeaponAnimState == WeaponAnimState::AimTransition) {
      if (m_TargetAimPitch == AimPitch::Up) {
        clipStart = 131;
        clipEnd = 136;
        m_CurrentAnimFrame += 0.55f;
        if (m_CurrentAnimFrame >= 136.0f) {
          m_AimPitch = AimPitch::Up;
          m_WeaponAnimState = WeaponAnimState::AimHold;
          m_CurrentAnimFrame = 137.0f;
        }
      } else if (m_TargetAimPitch == AimPitch::Down) {
        clipStart = 152;
        clipEnd = 157;
        m_CurrentAnimFrame += 0.55f;
        if (m_CurrentAnimFrame >= 157.0f) {
          m_AimPitch = AimPitch::Down;
          m_WeaponAnimState = WeaponAnimState::AimHold;
          m_CurrentAnimFrame = 158.0f;
        }
      }
    } else if (m_WeaponAnimState == WeaponAnimState::Firing) {
      if (m_AimPitch == AimPitch::Up) {
        clipStart = 137;
        clipEnd = 151;
        m_CurrentAnimFrame += 0.65f;
        if (m_CurrentAnimFrame > 151.0f) {
          m_WeaponAnimState =
              aimHeld ? WeaponAnimState::AimHold : WeaponAnimState::None;
          m_CurrentAnimFrame = aimHeld ? 137.0f : 0.0f;
        }
      } else if (m_AimPitch == AimPitch::Down) {
        clipStart = 158;
        clipEnd = 172;
        m_CurrentAnimFrame += 0.65f;
        if (m_CurrentAnimFrame > 172.0f) {
          m_WeaponAnimState =
              aimHeld ? WeaponAnimState::AimHold : WeaponAnimState::None;
          m_CurrentAnimFrame = aimHeld ? 158.0f : 0.0f;
        }
      } else {
        clipStart = 116;
        clipEnd = 130;
        m_CurrentAnimFrame += 0.65f;
        if (m_CurrentAnimFrame > 130.0f) {
          m_WeaponAnimState =
              aimHeld ? WeaponAnimState::AimHold : WeaponAnimState::None;
          m_CurrentAnimFrame = aimHeld ? 115.0f : 0.0f;
        }
      }
    } else if (m_WeaponAnimState == WeaponAnimState::Reloading) {
      clipStart = 173;
      clipEnd = 202;
      m_CurrentAnimFrame += 0.50f;
      if (m_CurrentAnimFrame >= 185.0f && m_AmmoCount < m_MaxAmmo) {
        m_AmmoCount = m_MaxAmmo;
        std::printf("[Combat] Beretta M92FS fully reloaded (15/15)!\n");
      }
      if (m_CurrentAnimFrame > 202.0f) {
        m_WeaponAnimState =
            aimHeld ? WeaponAnimState::AimHold : WeaponAnimState::None;
        m_CurrentAnimFrame = aimHeld ? 115.0f : 0.0f;
      }
    }
  } else {
    // -------------------------------------------------------------
    // 100% AUTHENTIC PS1 LOCOMOTION & KINEMATICS STATE MACHINE
    // (player.c:588-1020, pad.c:259-261, model.c:92-129)
    // -------------------------------------------------------------
    if (m_WeaponAnimState != WeaponAnimState::None &&
        m_WeaponAnimState != WeaponAnimState::Firing &&
        m_WeaponAnimState != WeaponAnimState::Reloading) {
      m_WeaponAnimState = WeaponAnimState::None;
      m_AimPitch = AimPitch::Straight;
      TriggerPoseBlend();
    }

    uint8_t floorZone =
        roomMgr ? roomMgr->LookupFootstepZone(m_Position.x, m_Position.z) : 45;

    // Determine target LocomotionMode
    LocomotionMode prevMode = m_LocoMode;
    LocomotionMode nextMode = prevMode;

    if (moveFwd && isRunning) {
      nextMode = LocomotionMode::RunFwd;
    } else if (moveFwd) {
      nextMode = LocomotionMode::WalkFwd;
    } else if (moveBack) {
      nextMode = LocomotionMode::WalkBack;
    } else if (prevMode == LocomotionMode::RunFwd) {
      // Authentic 4-tick (8 frames @ 60fps) Run Stop Skid (player.c:996-1015)
      nextMode = LocomotionMode::RunStopSkid;
      m_RunStopSkidFrames = 8;
      m_CurrentMoveSpeed = 105.0f; // 210 / 2
    } else if (prevMode == LocomotionMode::RunStopSkid) {
      if (m_RunStopSkidFrames > 0) {
        nextMode = LocomotionMode::RunStopSkid;
      } else if (turnLeft || turnRight) {
        nextMode = LocomotionMode::TurnInPlace;
      } else {
        nextMode = LocomotionMode::Idle;
      }
    } else if (turnLeft || turnRight) {
      nextMode = LocomotionMode::TurnInPlace;
    } else {
      nextMode = LocomotionMode::Idle;
    }

    // Handle state transitions, stop footsteps, and Walk <-> Run phase matching
    if (nextMode != prevMode) {
      TriggerPoseBlend();

      // Stop footstep when transitioning to Idle or RunStopSkid
      if (prevMode == LocomotionMode::WalkFwd ||
          prevMode == LocomotionMode::WalkBack ||
          prevMode == LocomotionMode::TurnInPlace) {
        if (nextMode == LocomotionMode::Idle) {
          g_AudioEngine.PlayFootstep(0, floorZone, 0.80f);
        }
      }

      // Authentic Walk <-> Run leg phase matching (player.c:943-947, 987-994)
      if (nextMode == LocomotionMode::RunFwd) {
        m_UseBaseEmdAnim = false;
        if (prevMode == LocomotionMode::WalkFwd) {
          float walkStep = m_CurrentAnimFrame - 35.0f;
          // Walk step <= 13 starts Run at step 10 (frame 73), else step 0 (frame 63)
          m_CurrentAnimFrame = (walkStep <= 13.0f) ? 73.0f : 63.0f;
        } else {
          m_CurrentAnimFrame = 63.0f;
        }
        prevFrame = m_CurrentAnimFrame;
      } else if (nextMode == LocomotionMode::WalkFwd) {
        m_UseBaseEmdAnim = false;
        if (prevMode == LocomotionMode::RunFwd) {
          float runStep = m_CurrentAnimFrame - 63.0f;
          // Run step <= 9 starts Walk at step 14 (frame 49), else step 0 (frame 35)
          m_CurrentAnimFrame = (runStep <= 9.0f) ? 49.0f : 35.0f;
        } else if (prevMode != LocomotionMode::TurnInPlace) {
          m_CurrentAnimFrame = 35.0f;
        }
        prevFrame = m_CurrentAnimFrame;
      } else if (nextMode == LocomotionMode::TurnInPlace) {
        m_UseBaseEmdAnim = false;
        if (prevMode != LocomotionMode::WalkFwd) {
          m_CurrentAnimFrame = 35.0f;
          prevFrame = 35.0f;
        }
      } else if (nextMode == LocomotionMode::WalkBack) {
        // Authentic Backstep uses CHAR10.EMD Anim 2 (frames 55..82 played forward)
        m_UseBaseEmdAnim = true;
        m_CurrentAnimFrame = 55.0f;
        prevFrame = 55.0f;
      } else if (nextMode == LocomotionMode::RunStopSkid) {
        // Authentic Run Stop Skid uses CHAR10.EMD Anim 0 (Frame 0)
        m_UseBaseEmdAnim = true;
        m_CurrentAnimFrame = 0.0f;
        prevFrame = 0.0f;
      } else if (nextMode == LocomotionMode::Idle) {
        if (m_WeaponEquipped) {
          m_UseBaseEmdAnim = false;
          m_CurrentAnimFrame = 19.0f;
        } else {
          m_UseBaseEmdAnim = true;
          m_CurrentAnimFrame = 0.0f;
        }
        prevFrame = m_CurrentAnimFrame;
      }

      m_LocoMode = nextMode;
    }

    // Authentic PS1 Turn Speeds (pad.c:259-261):
    // Stand = 96/tick (48/frame), Walk/Back = 40/tick (20/frame), Run = 48/tick (24/frame)
    constexpr float kAngleUnit = XM_2PI / 4096.0f;
    float turnSpeed = 48.0f * kAngleUnit;
    if (m_LocoMode == LocomotionMode::RunFwd) {
      turnSpeed = 24.0f * kAngleUnit;
    } else if (m_LocoMode == LocomotionMode::WalkFwd ||
               m_LocoMode == LocomotionMode::WalkBack) {
      turnSpeed = 20.0f * kAngleUnit;
    }

    if (turnLeft)
      m_FacingAngle -= turnSpeed;
    if (turnRight)
      m_FacingAngle += turnSpeed;

    // Advance Animation & Calculate Exact PS1 Move Speeds (30 Hz ticks -> 60 FPS * 0.5)
    m_IsMoving = false;
    m_IsRunning = false;
    float signedMoveDelta = 0.0f;

    switch (m_LocoMode) {
    case LocomotionMode::RunFwd: {
      m_IsMoving = true;
      m_IsRunning = true;
      m_UseBaseEmdAnim = false;
      clipStart = 63;
      clipEnd = 82;
      m_CurrentAnimFrame += 0.50f;
      if (m_CurrentAnimFrame >= 83.0f) {
        m_CurrentAnimFrame -= 20.0f;
      }
      // Footstep triggers at step 0 (frame 63) and step 10 (frame 73) (player.c:966-967)
      if ((prevFrame < 63.0f && m_CurrentAnimFrame >= 63.0f) ||
          (prevFrame > m_CurrentAnimFrame && m_CurrentAnimFrame >= 63.0f) ||
          (prevFrame < 73.0f && m_CurrentAnimFrame >= 73.0f)) {
        g_AudioEngine.PlayFootstep(1, floorZone, 0.90f);
      }
      m_CurrentMoveSpeed = 105.0f; // 0xD2 (210) units per 30 Hz tick = 105.0f per 60 FPS frame
      signedMoveDelta = m_CurrentMoveSpeed;
      break;
    }

    case LocomotionMode::RunStopSkid: {
      m_IsMoving = true;
      m_UseBaseEmdAnim = true;
      clipStart = 0;
      clipEnd = 0;
      m_CurrentAnimFrame = 0.0f;
      m_CurrentMoveSpeed -= 15.0f; // -0x1E (-30) per 30 Hz tick = -15.0f per 60 FPS frame
      if (m_CurrentMoveSpeed < 0.0f)
        m_CurrentMoveSpeed = 0.0f;
      signedMoveDelta = m_CurrentMoveSpeed;
      --m_RunStopSkidFrames;
      if (m_RunStopSkidFrames <= 0) {
        g_AudioEngine.PlayFootstep(1, floorZone, 0.85f);
        m_LocoMode = LocomotionMode::Idle;
        TriggerPoseBlend();
        if (m_WeaponEquipped) {
          m_UseBaseEmdAnim = false;
          m_CurrentAnimFrame = 19.0f;
          clipStart = 19;
          clipEnd = 34;
        }
      }
      break;
    }

    case LocomotionMode::WalkFwd: {
      m_IsMoving = true;
      m_UseBaseEmdAnim = false;
      clipStart = 35;
      clipEnd = 62;
      m_CurrentAnimFrame += 0.50f;
      if (m_CurrentAnimFrame >= 63.0f) {
        m_CurrentAnimFrame -= 28.0f;
      }
      // Footstep triggers at step 8 (frame 43) and step 22 (frame 57) (player.c:607, 678)
      if ((prevFrame < 43.0f && m_CurrentAnimFrame >= 43.0f) ||
          (prevFrame < 57.0f && m_CurrentAnimFrame >= 57.0f)) {
        g_AudioEngine.PlayFootstep(0, floorZone, 0.85f);
      }
      // Authentic Chris Walk Speed Curve (player.c:590, 630-647):
      // Base 0x5D (93)/tick = 46.5f/frame, modulated by tbl[0] = { 21, 23, 13, 14 }
      int step = static_cast<int>(m_CurrentAnimFrame) - 35;
      int stepMod = ((step >= 14) ? (step - 14) : step) + 21;
      float speed = 46.5f;
      if (stepMod >= 28) {
        speed -= 6.5f; // -13 / 2
        if (stepMod >= 30 && stepMod < 33) {
          speed -= 7.0f; // -14 / 2
        }
      }
      m_CurrentMoveSpeed = speed;
      signedMoveDelta = m_CurrentMoveSpeed;
      break;
    }

    case LocomotionMode::WalkBack: {
      m_IsMoving = true;
      m_UseBaseEmdAnim = true; // CHAR10.EMD Anim 2 (frames 55..82 played forward)
      clipStart = 55;
      clipEnd = 82;
      m_CurrentAnimFrame += 0.50f;
      if (m_CurrentAnimFrame >= 83.0f) {
        m_CurrentAnimFrame -= 28.0f;
      }
      // Footstep triggers at step 8 (frame 63) and step 22 (frame 77) (player.c:830-831)
      if ((prevFrame < 63.0f && m_CurrentAnimFrame >= 63.0f) ||
          (prevFrame < 77.0f && m_CurrentAnimFrame >= 77.0f)) {
        g_AudioEngine.PlayFootstep(0, floorZone, 0.78f);
      }
      m_CurrentMoveSpeed = 32.0f; // 0x40 (64) per 30 Hz tick = 32.0f per 60 FPS frame
      signedMoveDelta = -m_CurrentMoveSpeed;
      break;
    }

    case LocomotionMode::TurnInPlace: {
      m_UseBaseEmdAnim = false;
      clipStart = 35;
      clipEnd = 62;
      m_CurrentAnimFrame += 0.50f;
      if (m_CurrentAnimFrame >= 63.0f) {
        m_CurrentAnimFrame -= 28.0f;
      }
      // Footstep triggers at step 8 (frame 43) and step 22 (frame 57) (player.c:726-727)
      if ((prevFrame < 43.0f && m_CurrentAnimFrame >= 43.0f) ||
          (prevFrame < 57.0f && m_CurrentAnimFrame >= 57.0f)) {
        g_AudioEngine.PlayFootstep(0, floorZone, 0.80f);
      }
      m_CurrentMoveSpeed = 0.0f;
      break;
    }

    case LocomotionMode::Idle:
    default: {
      m_CurrentMoveSpeed = 0.0f;
      if (m_WeaponEquipped) {
        m_UseBaseEmdAnim = false;
        clipStart = 19;
        clipEnd = 34;
        if (m_CurrentAnimFrame < 19.0f || m_CurrentAnimFrame > 34.0f) {
          m_CurrentAnimFrame = 19.0f;
        }
        m_CurrentAnimFrame += 0.25f;
        if (m_CurrentAnimFrame >= 35.0f) {
          m_CurrentAnimFrame -= 16.0f;
        }
      } else {
        m_UseBaseEmdAnim = true;
        clipStart = 0;
        clipEnd = 0;
        m_CurrentAnimFrame = 0.0f;
      }
      break;
    }
    }

    float oldX = m_Position.x;
    float oldZ = m_Position.z;
    if (signedMoveDelta != 0.0f) {
      m_Position.x += std::sin(m_FacingAngle) * signedMoveDelta;
      m_Position.z += std::cos(m_FacingAngle) * signedMoveDelta;
      if (roomMgr) {
        roomMgr->ResolveCollision(m_Position.x, m_Position.z, oldX, oldZ,
                                  422.0f, m_Position.y);
      }
    }
  }

  BuildSkeletalPoseInterpolated(m_CurrentAnimFrame, clipStart, clipEnd);
  UploadCurrentPose();
}

void PlayerCharacter::Render(ID3D12GraphicsCommandList *cmdList,
                             Render::PS1Pipeline &pipeline,
                             const XMMATRIX &viewProj) {
  if (!m_IsLoaded || m_CurrentPoseVertices.empty())
    return;

  // In PS1 model space, Chris's chest faces +X. We subtract PI/2 so he faces
  // forward along +Z.
  XMMATRIX playerWorld =
      XMMatrixRotationY(m_FacingAngle - XM_PIDIV2) *
      XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);

  XMMATRIX worldViewProj = playerWorld * viewProj;

  pipeline.Bind(cmdList);

  Render::SceneConstants constants;
  constants.worldViewProj = worldViewProj;
  constants.prevWorldViewProj = (m_PrevWorldViewProj.r[0].m128_f32[0] != 0.0f)
                                    ? m_PrevWorldViewProj
                                    : worldViewProj;
  constants.screenResolution =
      XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = XMFLOAT4(
      1.0f, 1.0f, 1.0f, 1.0f); // Snapping, Affine, Dither, 15-bit color
  constants.blendMode = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
  constants.jitterOffset = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  pipeline.UpdateConstants(cmdList, constants);

  m_PrevWorldViewProj = worldViewProj;

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  cmdList->SetGraphicsRootDescriptorTable(1, m_SrvGpuHandle);

  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  cmdList->DrawInstanced(static_cast<UINT>(m_CurrentPoseVertices.size()), 1, 0,
                         0);
}

} // namespace Game
