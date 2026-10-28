#include "ps1_pipeline.h"
#include "ps1_ps_cso.h"
#include "ps1_vs_cso.h"
#include <cstdio>
#include <d3dcompiler.h>
#include <vector>

namespace Render {

bool PS1Pipeline::Initialize(ID3D12Device *device, DXGI_FORMAT rtvFormat,
                             DXGI_FORMAT velocityFormat) {
  // 1. Create Root Signature
  D3D12_ROOT_PARAMETER rootParameters[2] = {};

  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[0].Descriptor.ShaderRegister = 0;
  rootParameters[0].Descriptor.RegisterSpace = 0;
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  D3D12_DESCRIPTOR_RANGE descRange = {};
  descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  descRange.NumDescriptors = 2; // t0 (texture), t1 (clut)
  descRange.BaseShaderRegister = 0;
  descRange.RegisterSpace = 0;
  descRange.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
  rootParameters[1].DescriptorTable.pDescriptorRanges = &descRange;
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // Static Sampler (Point Filtering for crisp PS1 pixels)
  D3D12_STATIC_SAMPLER_DESC sampler = {};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.ShaderRegister = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  rootSigDesc.NumParameters = _countof(rootParameters);
  rootSigDesc.pParameters = rootParameters;
  rootSigDesc.NumStaticSamplers = 1;
  rootSigDesc.pStaticSamplers = &sampler;
  rootSigDesc.Flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signatureBlob;
  ComPtr<ID3DBlob> errorBlob;
  if (FAILED(D3D12SerializeRootSignature(&rootSigDesc,
                                         D3D_ROOT_SIGNATURE_VERSION_1,
                                         &signatureBlob, &errorBlob))) {
    if (errorBlob) {
      std::fprintf(stderr, "[PS1Pipeline] RootSig Error: %s\n",
                   (char *)errorBlob->GetBufferPointer());
    }
    return false;
  }

  if (FAILED(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                         signatureBlob->GetBufferSize(),
                                         IID_PPV_ARGS(&m_RootSignature)))) {
    return false;
  }

  // 2. Load / Compile Shaders with Motion Vector Support
  ComPtr<ID3DBlob> vsBlob, psBlob;
  const void *vsBytecode = g_ps1_vs_cso;
  size_t vsBytecodeSize = sizeof(g_ps1_vs_cso);
  const void *psBytecode = g_ps1_ps_cso;
  size_t psBytecodeSize = sizeof(g_ps1_ps_cso);

  UINT compileFlags =
      D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
  const wchar_t *shaderPathCandidates[] = {L"shaders/ps1_pipeline.hlsl",
                                           L"../shaders/ps1_pipeline.hlsl",
                                           L"port/shaders/ps1_pipeline.hlsl"};

  bool compiledRuntime = false;
  for (const wchar_t *path : shaderPathCandidates) {
    ComPtr<ID3DBlob> cVs, cPs, cErr;
    if (SUCCEEDED(D3DCompileFromFile(path, nullptr, nullptr, "VSMain", "vs_5_0",
                                     compileFlags, 0, &cVs, &cErr)) &&
        SUCCEEDED(D3DCompileFromFile(path, nullptr, nullptr, "PSMain", "ps_5_0",
                                     compileFlags, 0, &cPs, &cErr))) {
      vsBlob = cVs;
      psBlob = cPs;
      vsBytecode = vsBlob->GetBufferPointer();
      vsBytecodeSize = vsBlob->GetBufferSize();
      psBytecode = psBlob->GetBufferPointer();
      psBytecodeSize = psBlob->GetBufferSize();
      compiledRuntime = true;
      std::printf("[PS1Pipeline] Compiled motion-vector shader from %ls\n",
                  path);
      break;
    }
  }

  if (!compiledRuntime) {
    std::printf(
        "[PS1Pipeline] Using precompiled embedded shader bytecode fallback\n");
  }

  // 3. Define Vertex Input Layout
  D3D12_INPUT_ELEMENT_DESC inputElements[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

  // 4. Create Standard Single-Target PSO
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = {inputElements, _countof(inputElements)};
  psoDesc.pRootSignature = m_RootSignature.Get();
  psoDesc.VS = {vsBytecode, vsBytecodeSize};
  psoDesc.PS = {psBytecode, psBytecodeSize};

  psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  psoDesc.RasterizerState.DepthClipEnable = TRUE;

  psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
  psoDesc.BlendState.IndependentBlendEnable = FALSE;
  D3D12_RENDER_TARGET_BLEND_DESC &rtBlend = psoDesc.BlendState.RenderTarget[0];
  rtBlend.BlendEnable = TRUE;
  rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
  rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
  rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
  rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  psoDesc.DepthStencilState.DepthEnable = TRUE;
  psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  psoDesc.DepthStencilState.StencilEnable = FALSE;
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = rtvFormat;
  psoDesc.SampleDesc.Count = 1;

  if (FAILED(device->CreateGraphicsPipelineState(
          &psoDesc, IID_PPV_ARGS(&m_PipelineState)))) {
    std::fprintf(stderr, "[PS1Pipeline] Failed to create standard graphics "
                         "pipeline state object!\n");
    return false;
  }

  // 5. Create Dual-Target Motion Vector PSO (Target 0: Color, Target 1:
  // R16G16_FLOAT Velocity)
  if (velocityFormat != DXGI_FORMAT_UNKNOWN) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC mvDesc = psoDesc;
    mvDesc.NumRenderTargets = 2;
    mvDesc.RTVFormats[0] = rtvFormat;
    mvDesc.RTVFormats[1] = velocityFormat;

    D3D12_RENDER_TARGET_BLEND_DESC &rtVel = mvDesc.BlendState.RenderTarget[1];
    rtVel.BlendEnable = FALSE;
    rtVel.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    if (SUCCEEDED(device->CreateGraphicsPipelineState(
            &mvDesc, IID_PPV_ARGS(&m_MotionVectorPSO)))) {
      std::printf("[PS1Pipeline] Motion Vector dual-target PSO created (Color: "
                  "%u, Velocity: %u)\n",
                  rtvFormat, velocityFormat);
    }
  }

  // 6. Create Constant Buffer (256-byte aligned upload heap with ring buffer
  // for multiple draw calls)
  uint32_t cbAlignedSize = (sizeof(SceneConstants) + 255) & ~255;
  uint32_t totalBufferSize = cbAlignedSize * MAX_CONSTANT_SLOTS;

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = totalBufferSize;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  if (FAILED(device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_ConstantBuffer)))) {
    return false;
  }

  D3D12_RANGE readRange = {0, 0};
  m_ConstantBuffer->Map(0, &readRange,
                        reinterpret_cast<void **>(&m_CbvDataBegin));
  m_CurrentSlot = 0;

  std::printf("[PS1Pipeline] Pipeline state and root signature created "
              "successfully (%u CBV slots)\n",
              MAX_CONSTANT_SLOTS);
  return true;
}

void PS1Pipeline::Shutdown() {
  if (m_ConstantBuffer && m_CbvDataBegin) {
    m_ConstantBuffer->Unmap(0, nullptr);
    m_CbvDataBegin = nullptr;
  }
}

void PS1Pipeline::Bind(ID3D12GraphicsCommandList *cmdList) {
  cmdList->SetGraphicsRootSignature(m_RootSignature.Get());
  cmdList->SetPipelineState(m_PipelineState.Get());
}

void PS1Pipeline::UpdateConstants(ID3D12GraphicsCommandList *cmdList,
                                  const SceneConstants &constants) {
  if (!m_CbvDataBegin || !m_ConstantBuffer)
    return;

  uint32_t cbAlignedSize = (sizeof(SceneConstants) + 255) & ~255;
  uint32_t slot = m_CurrentSlot;
  m_CurrentSlot = (m_CurrentSlot + 1) % MAX_CONSTANT_SLOTS;

  uint8_t *slotPtr = m_CbvDataBegin + (slot * cbAlignedSize);
  std::memcpy(slotPtr, &constants, sizeof(SceneConstants));

  D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress =
      m_ConstantBuffer->GetGPUVirtualAddress() + (slot * cbAlignedSize);
  cmdList->SetGraphicsRootConstantBufferView(0, cbvGpuAddress);
}

} // namespace Render
