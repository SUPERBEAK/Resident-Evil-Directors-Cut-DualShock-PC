#include "enemy_zombie.h"
#include "audio_engine.h"
#include "psx_tim.h"
#include "room_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace Game {

void EnemyZombie::Shutdown() {
  if (m_VertexBuffer && m_VertexBufferMapped) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
  }
  m_VertexBuffer.Reset();
  m_Texture.Reset();
  m_SrvHeap.Reset();
  m_IsLoaded = false;
  m_IsAlive = false;
}

bool EnemyZombie::Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                            const std::wstring &emdPath) {
  m_Device = device;
  m_CmdQueue = cmdQueue;

  std::ifstream file(emdPath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::fprintf(stderr, "[EnemyZombie] Could not open EMD file: %ls\n", emdPath.c_str());
    return false;
  }

  size_t size = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  file.read(reinterpret_cast<char *>(buffer.data()), size);

  if (size < 64)
    return false;

  // 1. Read Capcom EMD Footer Offsets
  const uint32_t *end = reinterpret_cast<const uint32_t *>(buffer.data() + (size / 4) * 4);
  uint32_t timOff = (*(end - 1) >> 2) * 4;
  uint32_t tmdOff = (*(end - 2) >> 2) * 4;
  uint32_t emrOff = (*(end - 3) >> 2) * 4;
  uint32_t eddOff = (*(end - 4) >> 2) * 4;

  if (timOff >= size || tmdOff >= size || emrOff >= size) {
    std::fprintf(stderr, "[EnemyZombie] Invalid EMD footer offsets\n");
    return false;
  }

  // 2. Decode TIM Texture
  PSX::TimImage tim;
  if (!PSX::TimDecoder::LoadFromMemory(buffer.data() + timOff, size - timOff, tim)) {
    std::fprintf(stderr, "[EnemyZombie] Failed to load zombie TIM texture\n");
    return false;
  }

  // 3. Create GPU Texture2D & Upload Heap
  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = tim.width;
  texDesc.Height = tim.height;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

  D3D12_HEAP_PROPERTIES defHeap = {};
  defHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

  if (FAILED(device->CreateCommittedResource(
          &defHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
          IID_PPV_ARGS(&m_Texture)))) {
    return false;
  }

  UINT64 uploadBufferSize = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, nullptr, nullptr,
                               &uploadBufferSize);

  ComPtr<ID3D12Resource> textureUpload;
  D3D12_HEAP_PROPERTIES upHeap = {};
  upHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC upDesc = {};
  upDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  upDesc.Width = uploadBufferSize;
  upDesc.Height = 1;
  upDesc.DepthOrArraySize = 1;
  upDesc.MipLevels = 1;
  upDesc.SampleDesc.Count = 1;
  upDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  device->CreateCommittedResource(&upHeap, D3D12_HEAP_FLAG_NONE, &upDesc,
                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                 IID_PPV_ARGS(&textureUpload));

  uint8_t *pUploadData = nullptr;
  D3D12_RANGE readRange = {0, 0};
  textureUpload->Map(0, &readRange, reinterpret_cast<void **>(&pUploadData));
  for (UINT y = 0; y < tim.height; ++y) {
    std::memcpy(pUploadData + layout.Offset + y * layout.Footprint.RowPitch,
                tim.rgbaPixels.data() + y * tim.width, tim.width * sizeof(uint32_t));
  }
  textureUpload->Unmap(0, nullptr);

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
  srvHeapDesc.NumDescriptors = 1;
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_SrvHeap)))) {
    return false;
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;

  m_SrvGpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
  device->CreateShaderResourceView(m_Texture.Get(), &srvDesc,
                                   m_SrvHeap->GetCPUDescriptorHandleForHeapStart());

  // 5. Parse 15 Joint TMD Meshes
  size_t tmdSize = (timOff > tmdOff) ? (timOff - tmdOff) : (size - tmdOff);
  if (!PSX::TmdLoader::LoadFromMemory(buffer.data() + tmdOff, tmdSize, m_JointMeshes, 1.0f)) {
    std::fprintf(stderr, "[EnemyZombie] Failed to parse zombie TMD joint meshes\n");
    return false;
  }

  // 6. Parse EMR Armature and Joints
  uint16_t armOff = *reinterpret_cast<const uint16_t *>(buffer.data());
  uint16_t framesOff = *reinterpret_cast<const uint16_t *>(buffer.data() + 2);
  uint16_t frameSize = *reinterpret_cast<const uint16_t *>(buffer.data() + 6);

  m_Skeleton.resize(15);
  const int16_t *baseVecs = reinterpret_cast<const int16_t *>(buffer.data() + 8);
  for (int j = 0; j < 15; ++j) {
    m_Skeleton[j].baseOffset =
        XMFLOAT3(static_cast<float>(baseVecs[j * 3 + 0]),
                 static_cast<float>(baseVecs[j * 3 + 1]),
                 static_cast<float>(baseVecs[j * 3 + 2]));
    m_Skeleton[j].parentIndex = -1;
  }

  const uint8_t *armPtr = buffer.data() + armOff;
  for (int j = 0; j < 15; ++j) {
    uint8_t childCount = armPtr[j * 4 + 0];
    uint16_t childrenOff = *reinterpret_cast<const uint16_t *>(armPtr + j * 4 + 2);
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

  // 7. Parse EMR Animation Frames
  size_t animEnd = eddOff;
  if (animEnd > size || animEnd <= framesOff)
    animEnd = 0x580C;
  uint32_t numFrames = static_cast<uint32_t>((animEnd - framesOff) / frameSize);
  m_AnimFrames.resize(numFrames);

  constexpr float toRad = XM_2PI / 4096.0f;
  for (uint32_t f = 0; f < numFrames; ++f) {
    const uint8_t *fPtr = buffer.data() + framesOff + f * frameSize;
    const int16_t *root = reinterpret_cast<const int16_t *>(fPtr);
    m_AnimFrames[f].rootOffset =
        XMFLOAT3(static_cast<float>(root[0]), static_cast<float>(root[1]),
                 static_cast<float>(root[2]));

    const int16_t *rootRot = reinterpret_cast<const int16_t *>(fPtr + 6);
    m_AnimFrames[f].rootRotation =
        XMFLOAT3(static_cast<float>(rootRot[0]) * toRad,
                 static_cast<float>(rootRot[1]) * toRad,
                 static_cast<float>(rootRot[2]) * toRad);

    m_AnimFrames[f].jointRotations.resize(15);
    for (int j = 0; j < 15; ++j) {
      const int16_t *rot = reinterpret_cast<const int16_t *>(fPtr + 12 + j * 6);
      m_AnimFrames[f].jointRotations[j] =
          XMFLOAT3(static_cast<float>(rot[0]) * toRad,
                   static_cast<float>(rot[1]) * toRad,
                   static_cast<float>(rot[2]) * toRad);
    }
  }

  // 8. Count total render vertices & create upload vertex buffer
  size_t totalVertices = 0;
  for (const auto &mesh : m_JointMeshes) {
    totalVertices += mesh.vertices.size();
  }
  m_CurrentPoseVertices.resize(totalVertices);

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
  m_VertexBufferView.SizeInBytes = static_cast<UINT>(bufferDesc.Width);

  m_IsLoaded = true;
  m_IsAlive = true;
  m_Health = 40;
  m_State = ZombieAnimState::Idle;
  m_CurrentAnimFrame = 0.0f;

  BuildSkeletalPoseInterpolated(0.0f, 0, 18);
  UploadCurrentPose();

  std::printf("[EnemyZombie] Loaded zombie model: %zu joint meshes, %u anim frames, %zu render vertices\n",
              m_JointMeshes.size(), numFrames, totalVertices);
  return true;
}

void EnemyZombie::BuildSkeletalPoseInterpolated(float currentFrame,
                                               uint32_t clipStart,
                                               uint32_t clipEnd) {
  if (m_AnimFrames.empty() || m_JointMeshes.empty())
    return;

  uint32_t f0 = static_cast<uint32_t>(currentFrame);
  uint32_t f1 = f0 + 1;
  if (f0 < clipStart) f0 = clipStart;
  if (f0 > clipEnd)   f0 = clipEnd;
  if (f1 > clipEnd)   f1 = clipStart;

  float t = currentFrame - static_cast<float>(static_cast<uint32_t>(currentFrame));
  t = (std::max)(0.0f, (std::min)(1.0f, t));

  const auto &frameA = m_AnimFrames[f0 % m_AnimFrames.size()];
  const auto &frameB = m_AnimFrames[f1 % m_AnimFrames.size()];

  XMVECTOR rA = XMLoadFloat3(&frameA.rootOffset);
  XMVECTOR rB = XMLoadFloat3(&frameB.rootOffset);
  XMVECTOR rootInterp = XMVectorLerp(rA, rB, t);

  XMVECTOR rotA = XMLoadFloat3(&frameA.rootRotation);
  XMVECTOR rotB = XMLoadFloat3(&frameB.rootRotation);
  XMVECTOR rootRotInterp = XMVectorLerp(rotA, rotB, t);

  XMFLOAT3 rootRotF;
  XMStoreFloat3(&rootRotF, rootRotInterp);

  XMMATRIX rootTransform =
      XMMatrixRotationZ(rootRotF.z) * XMMatrixRotationX(rootRotF.x) *
      XMMatrixRotationY(rootRotF.y) *
      XMMatrixTranslationFromVector(rootInterp);

  std::vector<XMMATRIX> jointMatrices(15, XMMatrixIdentity());
  std::vector<bool> computed(15, false);

  for (int j = 0; j < 15; ++j) {
    if (m_Skeleton[j].parentIndex == -1) {
      XMVECTOR jRotA = XMLoadFloat3(&frameA.jointRotations[j]);
      XMVECTOR jRotB = XMLoadFloat3(&frameB.jointRotations[j]);
      XMFLOAT3 jRotF;
      XMStoreFloat3(&jRotF, XMVectorLerp(jRotA, jRotB, t));

      XMMATRIX local = XMMatrixRotationZ(jRotF.z) * XMMatrixRotationX(jRotF.x) *
                       XMMatrixRotationY(jRotF.y) *
                       XMMatrixTranslation(m_Skeleton[j].baseOffset.x,
                                           m_Skeleton[j].baseOffset.y,
                                           m_Skeleton[j].baseOffset.z);
      jointMatrices[j] = local * rootTransform;
      computed[j] = true;
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (int j = 0; j < 15; ++j) {
      if (!computed[j] && m_Skeleton[j].parentIndex >= 0 &&
          computed[m_Skeleton[j].parentIndex]) {
        XMVECTOR jRotA = XMLoadFloat3(&frameA.jointRotations[j]);
        XMVECTOR jRotB = XMLoadFloat3(&frameB.jointRotations[j]);
        XMFLOAT3 jRotF;
        XMStoreFloat3(&jRotF, XMVectorLerp(jRotA, jRotB, t));

        XMMATRIX local = XMMatrixRotationZ(jRotF.z) *
                         XMMatrixRotationX(jRotF.x) *
                         XMMatrixRotationY(jRotF.y) *
                         XMMatrixTranslation(m_Skeleton[j].baseOffset.x,
                                             m_Skeleton[j].baseOffset.y,
                                             m_Skeleton[j].baseOffset.z);
        jointMatrices[j] = local * jointMatrices[m_Skeleton[j].parentIndex];
        computed[j] = true;
        changed = true;
      }
    }
  }

  size_t outIdx = 0;
  for (size_t j = 0; j < m_JointMeshes.size(); ++j) {
    const auto &mesh = m_JointMeshes[j];
    XMMATRIX jointMat = (j < 15) ? jointMatrices[j] : jointMatrices[0];

    for (const auto &v : mesh.vertices) {
      XMVECTOR pos = XMVectorSet(v.position.x, v.position.y, v.position.z, 1.0f);
      XMVECTOR transformed = XMVector3Transform(pos, jointMat);

      Render::PS1Vertex outV = v;
      XMStoreFloat3(&outV.position, transformed);
      m_CurrentPoseVertices[outIdx++] = outV;
    }
  }


}

void EnemyZombie::UploadCurrentPose() {
  if (!m_VertexBufferMapped || m_CurrentPoseVertices.empty())
    return;
  std::memcpy(m_VertexBufferMapped, m_CurrentPoseVertices.data(),
              m_CurrentPoseVertices.size() * sizeof(Render::PS1Vertex));
}

void EnemyZombie::Update(float deltaTime, const XMFLOAT3 &playerPos, bool playerFired,
                         const RoomManager *roomMgr) {
  if (!m_IsLoaded)
    return;

  // 1. Dead corpse
  if (m_State == ZombieAnimState::Dead) {
    BuildSkeletalPoseInterpolated(127.0f, 103, 127);
    UploadCurrentPose();
    return;
  }

  // 2. Dying Collapse Animation
  if (m_State == ZombieAnimState::Dying) {
    m_CurrentAnimFrame += 0.45f;
    if (m_CurrentAnimFrame >= 127.0f) {
      m_CurrentAnimFrame = 127.0f;
      m_State = ZombieAnimState::Dead;
      m_IsAlive = false;
      std::printf("[EnemyZombie] Zombie collapsed dead.\n");
    }
    BuildSkeletalPoseInterpolated(m_CurrentAnimFrame, 103, 127);
    UploadCurrentPose();
    return;
  }

  // 3. Bullet Hit Recoil
  if (m_State == ZombieAnimState::HitRecoil) {
    m_CurrentAnimFrame += 0.55f;
    // Push backwards from gunshot impact
    float oldX = m_Position.x;
    float oldZ = m_Position.z;
    m_Position.x -= std::sin(m_FacingAngle) * (40.0f * deltaTime * 60.0f);
    m_Position.z -= std::cos(m_FacingAngle) * (40.0f * deltaTime * 60.0f);
    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, oldX, oldZ, m_CollisionRadius);
    }

    if (m_CurrentAnimFrame >= 102.0f) {
      m_State = ZombieAnimState::Shamble;
      m_CurrentAnimFrame = 19.0f;
    }
    BuildSkeletalPoseInterpolated(m_CurrentAnimFrame, 76, 102);
    UploadCurrentPose();
    return;
  }

  // 4. Neck Bite / Grapple
  if (m_State == ZombieAnimState::Bite) {
    m_CurrentAnimFrame += 0.35f;
    if (m_CurrentAnimFrame >= 75.0f) {
      m_State = ZombieAnimState::Shamble;
      m_CurrentAnimFrame = 19.0f;
      m_AttackCooldown = 2.0f; // 2 seconds between bites
    }
    BuildSkeletalPoseInterpolated(m_CurrentAnimFrame, 61, 75);
    UploadCurrentPose();
    return;
  }

  // 5. Lunge Attack
  if (m_State == ZombieAnimState::Lunge) {
    m_CurrentAnimFrame += 0.40f;
    float dx = playerPos.x - m_Position.x;
    float dz = playerPos.z - m_Position.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    if (dist < 450.0f && m_AttackCooldown <= 0.0f) {
      m_State = ZombieAnimState::Bite;
      m_CurrentAnimFrame = 61.0f;
      g_AudioEngine.PlayZombieBite();
      std::printf("[EnemyZombie] Zombie grappling player in bite attack!\n");
    } else if (m_CurrentAnimFrame >= 60.0f) {
      m_State = ZombieAnimState::Shamble;
      m_CurrentAnimFrame = 19.0f;
    }
    BuildSkeletalPoseInterpolated(m_CurrentAnimFrame, 40, 60);
    UploadCurrentPose();
    return;
  }

  // 6. Locomotion / AI Tracking (Idle vs Shamble)
  float dx = playerPos.x - m_Position.x;
  float dz = playerPos.z - m_Position.z;
  float dist = std::sqrt(dx * dx + dz * dz);

  if (m_AttackCooldown > 0.0f) {
    m_AttackCooldown -= deltaTime;
  }

  // Aggro check: player fired gun or entered room detection radius (< 6000 units)
  if (dist < 6000.0f || playerFired) {
    m_State = ZombieAnimState::Shamble;
  }

  if (m_State == ZombieAnimState::Shamble) {
    // Smoothly turn to face player
    float targetAngle = std::atan2(dx, dz);
    float angleDiff = targetAngle - m_FacingAngle;
    while (angleDiff >  DirectX::XM_PI) angleDiff -= DirectX::XM_2PI;
    while (angleDiff < -DirectX::XM_PI) angleDiff += DirectX::XM_2PI;

    m_FacingAngle += angleDiff * (std::min)(1.0f, deltaTime * 1.8f);

    // Shamble forward along ground plane
    float moveSpeed = 42.0f; // Classic sluggish zombie pace
    float oldX = m_Position.x;
    float oldZ = m_Position.z;
    m_Position.x += std::sin(m_FacingAngle) * (moveSpeed * deltaTime);
    m_Position.z += std::cos(m_FacingAngle) * (moveSpeed * deltaTime);

    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, oldX, oldZ, m_CollisionRadius);
    }

    // Advance shamble walk animation (Clip 0: 19..39)
    m_CurrentAnimFrame += 0.32f;
    if (m_CurrentAnimFrame < 19.0f || m_CurrentAnimFrame > 39.0f) {
      m_CurrentAnimFrame = 19.0f;
    }

    // Groan timer
    m_GroanTimer -= deltaTime;
    if (m_GroanTimer <= 0.0f) {
      if (m_IsAlive && dist < 1800.0f) {
        g_AudioEngine.PlayZombieGroan(0.85f);
      }
      m_GroanTimer = 6.0f + (static_cast<float>(rand() % 40) / 10.0f);
    }

    // If within strike distance (< 520 units), initiate lunge!
    if (dist < 520.0f && m_AttackCooldown <= 0.0f) {
      m_State = ZombieAnimState::Lunge;
      m_CurrentAnimFrame = 40.0f;
    }

    BuildSkeletalPoseInterpolated(m_CurrentAnimFrame, 19, 39);
  } else {
    // Idle Groan / Sway (Clip 2: 0..18)
    m_CurrentAnimFrame += 0.25f;
    if (m_CurrentAnimFrame > 18.0f) {
      m_CurrentAnimFrame = 0.0f;
    }
    BuildSkeletalPoseInterpolated(m_CurrentAnimFrame, 0, 18);
  }

  UploadCurrentPose();
}

bool EnemyZombie::CheckBulletHit(const XMFLOAT3 &gunPos, float gunFacingAngle,
                                 int pitchIndex, int damage) {
  if (!m_IsLoaded || !m_IsAlive || m_State == ZombieAnimState::Dying ||
      m_State == ZombieAnimState::Dead)
    return false;

  float dx = m_Position.x - gunPos.x;
  float dz = m_Position.z - gunPos.z;
  float dist = std::sqrt(dx * dx + dz * dz);

  // Maximum handgun range ~6000 units
  if (dist > 6000.0f || dist < 10.0f)
    return false;

  float angleToZombie = std::atan2(dx, dz);
  float angleDiff = angleToZombie - gunFacingAngle;
  while (angleDiff >  DirectX::XM_PI) angleDiff -= DirectX::XM_2PI;
  while (angleDiff < -DirectX::XM_PI) angleDiff += DirectX::XM_2PI;

  // Aim cone tolerance: ~24 degrees (0.42 radians)
  if (std::abs(angleDiff) < 0.42f) {
    m_Health -= damage;
    std::printf("[Combat] Bullet hit zombie! (Damage: %d | Health Remaining: %d)\n",
                damage, (std::max)(0, m_Health));

    if (m_Health <= 0) {
      m_Health = 0;
      m_State = ZombieAnimState::Dying;
      m_CurrentAnimFrame = 103.0f;
      g_AudioEngine.PlayZombieDeath();
      std::printf("[Combat] Zombie killed! Collapsing backwards.\n");
    } else {
      m_State = ZombieAnimState::HitRecoil;
      m_CurrentAnimFrame = 76.0f;
      g_AudioEngine.PlayZombieHit();
    }
    return true;
  }

  return false;
}

void EnemyZombie::Render(ID3D12GraphicsCommandList *cmdList,
                         Render::PS1Pipeline &pipeline,
                         const XMMATRIX &viewProj) {
  if (!m_IsLoaded || m_CurrentPoseVertices.empty() || !m_VertexBuffer)
    return;

  XMMATRIX world = XMMatrixRotationY(m_FacingAngle - DirectX::XM_PIDIV2) *
                   XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);

  pipeline.Bind(cmdList);

  Render::SceneConstants constants;
  constants.worldViewProj = XMMatrixTranspose(world * viewProj);
  constants.prevWorldViewProj = constants.worldViewProj;
  constants.screenResolution = XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
  constants.blendMode = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
  constants.jitterOffset = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  pipeline.UpdateConstants(cmdList, constants);

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  cmdList->SetGraphicsRootDescriptorTable(1, m_SrvGpuHandle);

  cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(static_cast<UINT>(m_CurrentPoseVertices.size()), 1, 0, 0);
}

} // namespace Game
