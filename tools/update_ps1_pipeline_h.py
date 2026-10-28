content = """#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Render {

struct PS1Vertex {
  XMFLOAT3 position;
  XMFLOAT4 color;
  XMFLOAT2 uv;
};

struct SceneConstants {
  XMMATRIX worldViewProj;
  XMMATRIX prevWorldViewProj; // Previous frame WVP matrix for velocity buffer
  XMFLOAT4 screenResolution;  // xy: res (e.g. 320, 240), zw: 1.0 / res
  XMFLOAT4 renderFlags;       // x: snapEnabled, y: affineEnabled, z: ditherEnabled, w: color15BitEnabled
  XMFLOAT4 blendMode;         // x: semiTransMode, y: isSemiTrans, z: writeMotionVectors
  XMFLOAT4 jitterOffset;      // xy: current subpixel jitter, zw: previous jitter
};

class PS1Pipeline {
public:
  PS1Pipeline() = default;
  ~PS1Pipeline() = default;

  bool Initialize(ID3D12Device *device, DXGI_FORMAT rtvFormat, DXGI_FORMAT velocityFormat = DXGI_FORMAT_UNKNOWN);
  void Shutdown();

  void Bind(ID3D12GraphicsCommandList *cmdList);
  void UpdateConstants(ID3D12GraphicsCommandList *cmdList,
                       const SceneConstants &constants);

  ID3D12RootSignature *GetRootSignature() const {
    return m_RootSignature.Get();
  }
  ID3D12PipelineState *GetPipelineState() const {
    return m_PipelineState.Get();
  }
  ID3D12PipelineState *GetMotionVectorPipelineState() const {
    return m_MotionVectorPSO.Get();
  }

private:
  ComPtr<ID3D12RootSignature> m_RootSignature;
  ComPtr<ID3D12PipelineState> m_PipelineState;
  ComPtr<ID3D12PipelineState> m_MotionVectorPSO;
  ComPtr<ID3D12Resource> m_ConstantBuffer;
  uint8_t *m_CbvDataBegin = nullptr;
};

} // namespace Render
"""
with open("port/include/ps1_pipeline.h", "wb") as f:
    f.write(content.encode("utf-8"))
print("Updated ps1_pipeline.h successfully")
