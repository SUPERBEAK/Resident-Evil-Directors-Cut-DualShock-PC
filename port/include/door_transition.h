#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace Render {
class PS1Pipeline;
struct PS1Vertex;
} // namespace Render

namespace Game {

struct DoorMeshObject {
  uint32_t vertexOffset = 0;
  uint32_t vertexCount = 0;
  bool isKnob = false;
  bool isBackKnob = false;
  DirectX::XMFLOAT3 attachOffset = {0.0f, 0.0f, 0.0f};
};

class DoorTransition {
public:
  DoorTransition() = default;
  ~DoorTransition() = default;

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  const std::wstring &itemM1Dir);
  void Shutdown();

  bool IsActive() const { return m_IsActive; }

  void StartTransition(uint32_t doorSlot, uint32_t doorType,
                       std::function<void()> onComplete);
  void Update(float deltaTime, uint32_t buttons);
  void Render(ID3D12GraphicsCommandList *cmdList,
              Render::PS1Pipeline &pipeline);

private:
  bool LoadDoorAsset(uint32_t doorSlot);
  void RebuildDynamicVertices();

  bool m_IsActive = false;
  ID3D12Device *m_Device = nullptr;
  ID3D12CommandQueue *m_CmdQueue = nullptr;
  std::wstring m_ItemM1Dir;

  uint32_t m_CurrentSlot = 0;
  float m_Timeline = 0.0f;
  float m_DoorAngle = 0.0f;
  float m_HandleAngle = 0.0f;
  float m_CameraX = 6800.0f;
  float m_FadeAlpha = 0.0f;
  bool m_HandledCutover = false;

  std::function<void()> m_OnComplete = nullptr;

  std::vector<DoorMeshObject> m_Objects;
  std::vector<Render::PS1Vertex> m_BaseVertices;
  std::vector<Render::PS1Vertex> m_FrameVertices;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_DoorTexture;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
  D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpuHandle = {};

  Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
  void *m_VertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
  uint32_t m_VertexCount = 0;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_FadeVertexBuffer;
  void *m_FadeVertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_FadeVertexBufferView = {};
};

extern DoorTransition g_DoorTransition;

} // namespace Game
