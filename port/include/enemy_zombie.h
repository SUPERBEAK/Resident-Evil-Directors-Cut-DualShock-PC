#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "ps1_pipeline.h"
#include "psx_tmd.h"
#include "player_character.h"

class RoomManager;

namespace Game {

using Microsoft::WRL::ComPtr;
using namespace DirectX;

enum class ZombieAnimState {
  Idle,        // Idle standing / groaning sway (Clip 2: frames 0..18)
  Shamble,     // Slow shuffling walk tracking player (Clip 0: frames 19..39)
  Lunge,       // Arms forward lunge (Clip 3: frames 40..60)
  Bite,        // Neck bite grapple (Clip 4: frames 61..75)
  HitRecoil,   // Bullet impact stagger backwards (Clip 5: frames 76..102)
  Dying,       // Collapse backwards to floor (Clip 7: frames 103..127)
  Dead         // Static corpse on floor
};

class EnemyZombie {
public:
  EnemyZombie() = default;
  ~EnemyZombie() { Shutdown(); }

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  const std::wstring &emdPath);
  void Shutdown();

  void Update(float deltaTime, const XMFLOAT3 &playerPos, bool playerFired,
              const RoomManager *roomMgr = nullptr);
  void Render(ID3D12GraphicsCommandList *cmdList, Render::PS1Pipeline &pipeline,
              const XMMATRIX &viewProj);

  // Ballistics & Combat Hitscan
  bool CheckBulletHit(const XMFLOAT3 &gunPos, float gunFacingAngle,
                      int pitchIndex, int damage);

  void SetPosition(float x, float y, float z) { m_Position = XMFLOAT3(x, y, z); }
  void SetFacingAngle(float radians) { m_FacingAngle = radians; }

  XMFLOAT3 GetPosition() const { return m_Position; }
  float GetFacingAngle() const { return m_FacingAngle; }
  bool IsLoaded() const { return m_IsLoaded; }
  bool IsAlive() const { return m_IsAlive; }
  int GetHealth() const { return m_Health; }
  ZombieAnimState GetState() const { return m_State; }
  float GetCollisionRadius() const { return m_CollisionRadius; }

private:
  void BuildSkeletalPose(uint32_t frameIdx);
  void BuildSkeletalPoseInterpolated(float currentFrame, uint32_t clipStart,
                                     uint32_t clipEnd);
  void UploadCurrentPose();

  ComPtr<ID3D12Device> m_Device;
  ComPtr<ID3D12CommandQueue> m_CmdQueue;
  ComPtr<ID3D12Resource> m_Texture;
  ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
  D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpuHandle = {};

  ComPtr<ID3D12Resource> m_VertexBuffer;
  void *m_VertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

  std::vector<PSX::TmdMesh> m_JointMeshes;
  std::vector<JointNode> m_Skeleton;
  std::vector<PlayerAnimFrame> m_AnimFrames;
  std::vector<Render::PS1Vertex> m_CurrentPoseVertices;

  XMFLOAT3 m_Position = {0.0f, 0.0f, 0.0f};
  float m_FacingAngle = 0.0f;
  float m_CollisionRadius = 380.0f;

  ZombieAnimState m_State = ZombieAnimState::Idle;
  int m_Health = 40; // 3-4 handgun shots to defeat
  bool m_IsAlive = true;
  bool m_IsLoaded = false;

  float m_CurrentAnimFrame = 0.0f;
  float m_AnimSpeed = 0.35f;
  float m_GroanTimer = 2.0f;
  float m_AttackCooldown = 0.0f;
};

} // namespace Game
