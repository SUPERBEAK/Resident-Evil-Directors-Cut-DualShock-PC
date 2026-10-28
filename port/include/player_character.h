#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "ps1_pipeline.h"
#include "psx_tmd.h"

class RoomManager;

namespace Game {

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct JointNode {
  XMFLOAT3 baseOffset = {0.0f, 0.0f, 0.0f};
  int parentIndex = -1;
};

struct PlayerAnimFrame {
  XMFLOAT3 rootOffset = {0.0f, 0.0f, 0.0f};
  XMFLOAT3 rootRotation = {0.0f, 0.0f, 0.0f};
  std::vector<XMFLOAT3> jointRotations;
};

enum class WeaponAnimState {
  None, // Relaxed locomotion holding handgun at side (Clip 0 idle, Clip 2 walk,
        // Clip 4 run)
  Raise,   // Raising handgun straight ahead (Clip 5: frames 108..115)
  AimHold, // Holding aim position (Clip 7 straight: 115, Clip 10 up: 137, Clip
           // 13 down: 158)
  AimTransition, // Pivoting arms between aim elevations (Clip 8 up: 131..136,
                 // Clip 11 down: 152..157)
  Firing, // Muzzle recoil kick (Clip 6 straight: 116..130, Clip 9 up: 137..151,
          // Clip 12 down: 158..172)
  Reloading, // Magazine eject and reload (Clip 14: 173..202)
};

enum class AimPitch { Straight, Up, Down };

enum class LocomotionMode {
  Idle,
  WalkFwd,
  RunFwd,
  RunStopSkid,
  WalkBack,
  TurnInPlace
};

class PlayerCharacter {
public:
  PlayerCharacter() = default;
  ~PlayerCharacter() { Shutdown(); }

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  const std::wstring &emdPath);
  bool LoadEquippedWeapon(const std::wstring &emwPath,
                          const std::wstring &tmdPath);
  void Shutdown();

  void Update(float deltaTime, uint32_t padButtons,
              const RoomManager *roomMgr = nullptr);
  void Render(ID3D12GraphicsCommandList *cmdList, Render::PS1Pipeline &pipeline,
              const XMMATRIX &viewProj);

  void SetPosition(float x, float y, float z) {
    m_Position = XMFLOAT3(x, y, z);
  }
  void SetFacingAngle(float radians) { m_FacingAngle = radians; }

  XMFLOAT3 GetPosition() const { return m_Position; }
  float GetFacingAngle() const { return m_FacingAngle; }
  bool IsLoaded() const { return m_IsLoaded; }

  int GetHealth() const { return m_Health; }
  int GetMaxHealth() const { return m_MaxHealth; }
  void SetHealth(int hp) { m_Health = hp; }
  void SetMaxHealth(int maxHp) { m_MaxHealth = maxHp; }
  void SetArmed(bool armed) { m_WeaponEquipped = armed; }

  // Weapon & Combat Getters
  bool IsWeaponEquipped() const { return m_WeaponEquipped; }
  bool IsAiming() const { return m_WeaponAnimState != WeaponAnimState::None; }
  bool IsFiring() const { return m_WeaponAnimState == WeaponAnimState::Firing; }
  AimPitch GetAimPitch() const { return m_AimPitch; }
  WeaponAnimState GetWeaponAnimState() const { return m_WeaponAnimState; }
  int GetAmmo() const { return m_AmmoCount; }
  int GetMaxAmmo() const { return m_MaxAmmo; }
  bool JustFiredBullet() {
    bool fired = m_JustFiredBullet;
    m_JustFiredBullet = false;
    return fired;
  }
  void Reload();
  void FireWeapon();

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
  std::vector<PSX::TmdMesh>
      m_WeaponMeshes; // Handgun 3D model attached to Joint 8
  std::vector<JointNode> m_Skeleton;
  std::vector<PlayerAnimFrame> m_AnimFrames;
  std::vector<PlayerAnimFrame> m_UnarmedWeaponAnimFrames; // 108 frames from W00.EMW
  std::vector<PlayerAnimFrame> m_WeaponAnimFrames;        // 203 frames from W02.EMW
  std::vector<Render::PS1Vertex> m_CurrentPoseVertices;

  LocomotionMode m_LocoMode = LocomotionMode::Idle;
  bool m_UseBaseEmdAnim = true;
  int m_RunStopSkidFrames = 0;
  float m_CurrentMoveSpeed = 0.0f;

  // Authentic PS1 3-tick (6 frames @ 60fps) joint_move pose cross-fade state
  float m_PoseBlendAlpha = 0.0f;
  XMFLOAT3 m_LastRootOffset = {0.0f, -1807.0f, 0.0f};
  std::vector<XMFLOAT3> m_LastJointRotations;
  XMFLOAT3 m_BlendSrcRootOffset = {0.0f, -1807.0f, 0.0f};
  std::vector<XMFLOAT3> m_BlendSrcJointRotations;
  void TriggerPoseBlend();

  XMFLOAT3 m_Position = {5274.0f, -2430.0f,
                         5652.0f}; // Default floor in Mansion Save Room
  float m_FacingAngle = 0.0f;
  float m_CurrentAnimFrame = 0.0f;
  bool m_IsMoving = false;
  bool m_IsRunning = false;
  bool m_IsLoaded = false;

  // Weapon Combat State
  bool m_WeaponEquipped = false;
  int m_Health = 140;
  int m_MaxHealth = 140;
  int m_AmmoCount = 15;
  int m_MaxAmmo = 15;
  WeaponAnimState m_WeaponAnimState = WeaponAnimState::None;
  AimPitch m_AimPitch = AimPitch::Straight;
  AimPitch m_TargetAimPitch = AimPitch::Straight;
  float m_RumbleTimer = 0.0f;
  bool m_JustFiredBullet = false;

  XMMATRIX m_PrevWorldViewProj = XMMatrixIdentity();
};

} // namespace Game
