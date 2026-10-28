import re

# 1. Patch dx12_renderer.cpp
with open('port/src/renderer/dx12_renderer.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Add DSV and DepthStencil creation to CreateRenderTargets
old_rtv_loop = """    m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr,
                                     rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }
  return true;
}"""

new_rtv_loop = """    m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr,
                                     rtvHandle);
    rtvHandle.ptr += m_RtvDescriptorSize;
  }

  // Create DSV Heap
  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  if (FAILED(m_Device->CreateDescriptorHeap(&dsvHeapDesc,
                                            IID_PPV_ARGS(&m_DsvHeap)))) {
    return false;
  }

  // Create Depth-Stencil Texture (D32_FLOAT)
  D3D12_HEAP_PROPERTIES depthHeapProps = {};
  depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_RESOURCE_DESC depthDesc = {};
  depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depthDesc.Alignment = 0;
  depthDesc.Width = m_Width;
  depthDesc.Height = m_Height;
  depthDesc.DepthOrArraySize = 1;
  depthDesc.MipLevels = 1;
  depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
  depthDesc.SampleDesc.Count = 1;
  depthDesc.SampleDesc.Quality = 0;
  depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE depthClear = {};
  depthClear.Format = DXGI_FORMAT_D32_FLOAT;
  depthClear.DepthStencil.Depth = 1.0f;
  depthClear.DepthStencil.Stencil = 0;

  if (FAILED(m_Device->CreateCommittedResource(
          &depthHeapProps, D3D12_HEAP_FLAG_NONE, &depthDesc,
          D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear,
          IID_PPV_ARGS(&m_DepthStencil)))) {
    std::fprintf(stderr, "[DX12] Failed to create Depth Stencil texture!\\n");
    return false;
  }

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  m_Device->CreateDepthStencilView(
      m_DepthStencil.Get(), &dsvDesc,
      m_DsvHeap->GetCPUDescriptorHandleForHeapStart());

  return true;
}"""

content = content.replace(old_rtv_loop, new_rtv_loop)

# Update BeginFrame to bind DSV and clear depth
old_begin_frame = """  m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
  m_CommandList->RSSetViewports(1, &m_Viewport);
  m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

  const float clearColor[] = {0.05f, 0.05f, 0.07f, 1.0f};
  m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}"""

new_begin_frame = """  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
      m_DsvHeap->GetCPUDescriptorHandleForHeapStart();

  m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
  m_CommandList->RSSetViewports(1, &m_Viewport);
  m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

  const float clearColor[] = {0.05f, 0.05f, 0.07f, 1.0f};
  m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f,
                                       0, 0, nullptr);
}"""

content = content.replace(old_begin_frame, new_begin_frame)

# Update Shutdown to reset depth resources
old_shutdown = """void DX12Renderer::Shutdown() {
  WaitForGpu();
  m_Pipeline.Shutdown();"""

new_shutdown = """void DX12Renderer::Shutdown() {
  WaitForGpu();
  m_Pipeline.Shutdown();
  m_DepthStencil.Reset();
  m_DsvHeap.Reset();"""

content = content.replace(old_shutdown, new_shutdown)

# Update Resize to reset depth resources before recreating
old_resize = """  WaitForGpu();
  for (UINT i = 0; i < FRAME_COUNT; i++) {
    m_RenderTargets[i].Reset();
  }"""

new_resize = """  WaitForGpu();
  for (UINT i = 0; i < FRAME_COUNT; i++) {
    m_RenderTargets[i].Reset();
  }
  m_DepthStencil.Reset();
  m_DsvHeap.Reset();"""

content = content.replace(old_resize, new_resize)

with open('port/src/renderer/dx12_renderer.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("dx12_renderer.cpp patched successfully!")


# 2. Patch ps1_pipeline.cpp
with open('port/src/renderer/ps1_pipeline.cpp', 'r', encoding='utf-8') as f:
    p_content = f.read()

old_pipeline_depth = """  psoDesc.DepthStencilState.DepthEnable = FALSE;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = rtvFormat;
  psoDesc.SampleDesc.Count = 1;"""

new_pipeline_depth = """  psoDesc.DepthStencilState.DepthEnable = TRUE;
  psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  psoDesc.DepthStencilState.StencilEnable = FALSE;
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = rtvFormat;
  psoDesc.SampleDesc.Count = 1;"""

p_content = p_content.replace(old_pipeline_depth, new_pipeline_depth)

with open('port/src/renderer/ps1_pipeline.cpp', 'w', encoding='utf-8') as f:
    f.write(p_content)
print("ps1_pipeline.cpp patched successfully!")


# 3. Patch psx_tim.cpp
with open('port/src/hal/psx_tim.cpp', 'r', encoding='utf-8') as f:
    t_content = f.read()

old_tim_c15 = """uint32_t TimDecoder::C15ToRgba(uint16_t c15) {
  if (c15 == 0)
    return 0x00000000; // Transparent black

  uint32_t r = ((c15 & 0x1F) * 255) / 31;
  uint32_t g = (((c15 >> 5) & 0x1F) * 255) / 31;
  uint32_t b = (((c15 >> 10) & 0x1F) * 255) / 31;
  uint32_t a = 0xFF; // Full opacity
  return (a << 24) | (b << 16) | (g << 8) | r;
}"""

new_tim_c15 = """uint32_t TimDecoder::C15ToRgba(uint16_t c15) {
  uint32_t r = ((c15 & 0x1F) * 255) / 31;
  uint32_t g = (((c15 >> 5) & 0x1F) * 255) / 31;
  uint32_t b = (((c15 >> 10) & 0x1F) * 255) / 31;
  uint32_t a = 0xFF; // Full opacity: 3D character models are solid opaque
  return (a << 24) | (b << 16) | (g << 8) | r;
}"""

t_content = t_content.replace(old_tim_c15, new_tim_c15)

with open('port/src/hal/psx_tim.cpp', 'w', encoding='utf-8') as f:
    f.write(t_content)
print("psx_tim.cpp patched successfully!")


# 4. Patch player_character.cpp
with open('port/src/game/player_character.cpp', 'r', encoding='utf-8') as f:
    pc_content = f.read()

# Fix weapon equipped initial frame and pose
old_equip_init = """  m_CurrentAnimFrame = 0.0f;
  BuildSkeletalPoseInterpolated(0.0f, 0, 34);
  UploadCurrentPose();"""

new_equip_init = """  m_CurrentAnimFrame = 19.0f;
  BuildSkeletalPoseInterpolated(19.0f, 19, 34);
  UploadCurrentPose();"""

pc_content = pc_content.replace(old_equip_init, new_equip_init)

# Fix Object 15 parenting to Joint 0 (Pelvis)
old_joint_calc = """  for (size_t j = 0; j < m_JointMeshes.size() && j < m_Skeleton.size(); ++j) {
    jointMatrices[j] =
        ComputeJointMatrix(ComputeJointMatrix, static_cast<int>(j));
  }"""

new_joint_calc = """  for (size_t j = 0; j < m_JointMeshes.size() && j < m_Skeleton.size(); ++j) {
    jointMatrices[j] =
        ComputeJointMatrix(ComputeJointMatrix, static_cast<int>(j));
  }
  if (m_JointMeshes.size() > 15 && jointMatrices.size() > 0) {
    jointMatrices[15] = jointMatrices[0]; // Parent holster/sheath to Joint 0 (Pelvis)
  }"""

pc_content = pc_content.replace(old_joint_calc, new_joint_calc)

# Fix armed idle animation loop from 0..34 to authentic 19..34
old_armed_idle = """      } else {
        clipStart = 0;
        clipEnd = 34;
        if (m_CurrentAnimFrame > 34.0f) {
          m_CurrentAnimFrame = 0.0f;
        }
        m_CurrentAnimFrame += 0.20f;
        if (m_CurrentAnimFrame > 34.0f) {
          m_CurrentAnimFrame = 0.0f;
        }
      }"""

new_armed_idle = """      } else {
        clipStart = 19;
        clipEnd = 34;
        if (m_CurrentAnimFrame < 19.0f || m_CurrentAnimFrame > 34.0f) {
          m_CurrentAnimFrame = 19.0f;
        }
        m_CurrentAnimFrame += 0.20f;
        if (m_CurrentAnimFrame > 34.0f) {
          m_CurrentAnimFrame = 19.0f;
        }
      }"""

pc_content = pc_content.replace(old_armed_idle, new_armed_idle)

with open('port/src/game/player_character.cpp', 'w', encoding='utf-8') as f:
    f.write(pc_content)
print("player_character.cpp patched successfully!")
