import os

path = r'port/src/renderer/dx12_renderer.cpp'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Update Initialize
old_init = """  // Initialize PS1 Authentic HLSL Pipeline
  if (!m_Pipeline.Initialize(m_Device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM)) {
    return false;
  }

  if (!CreateTestGeometry())
    return false;"""

new_init = """  // Initialize PS1 Authentic HLSL Pipeline (Dual target: Color + R16G16_FLOAT Velocity)
  if (!m_Pipeline.Initialize(m_Device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16_FLOAT)) {
    return false;
  }

  // Initialize Background Loader
  if (!m_BgLoader.Initialize(m_Device.Get(), m_CommandQueue.Get())) {
    std::fprintf(stderr, "[DX12] Failed to initialize background loader\\n");
    return false;
  }

  // Initialize DLSS / Spatial Upscaler Hook
  m_DlssHook.Initialize(m_Device.Get(), 320, 240, m_Width, m_Height);

  if (!CreateTestGeometry())
    return false;"""

if old_init in text:
    text = text.replace(old_init, new_init)
    print("Replaced Initialize block")
else:
    print("Initialize block already updated or not found")

# 2. Fix dummy texture heap
old_tex = """  if (FAILED(m_Device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_DummyTexture)))) {
    return false;
  }

  uint32_t whitePixel = 0xFFFFFFFF;
  D3D12_SUBRESOURCE_DATA subData = {};
  subData.pData = &whitePixel;
  subData.RowPitch = 4;
  subData.SlicePitch = 4;"""

new_tex = """  D3D12_HEAP_PROPERTIES defaultHeapProps = {};
  defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

  if (FAILED(m_Device->CreateCommittedResource(
          &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
          IID_PPV_ARGS(&m_DummyTexture)))) {
    std::fprintf(stderr, "[DX12] Failed to create dummy texture\\n");
    return false;
  }"""

if old_tex in text:
    text = text.replace(old_tex, new_tex)
    print("Replaced dummy texture block")
else:
    print("Dummy texture block already updated or not found")

# 3. Append missing methods before closing brace if not present
if "RenderBackground()" not in text:
    methods = """
void DX12Renderer::RenderBackground() {
  m_Pipeline.Bind(m_CommandList.Get());

  SceneConstants constants;
  constants.worldViewProj = XMMatrixIdentity();
  constants.prevWorldViewProj = XMMatrixIdentity();
  constants.screenResolution =
      XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
  constants.blendMode = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  constants.jitterOffset = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  m_Pipeline.UpdateConstants(m_CommandList.Get(), constants);

  m_BgLoader.Draw(m_CommandList.Get());
}

bool DX12Renderer::LoadRoomBackground(const std::wstring &bssFilePath,
                                      uint32_t cutIndex) {
  return m_BgLoader.LoadRoomCut(bssFilePath, cutIndex);
}

bool DX12Renderer::LoadTmdModel(const std::wstring &tmdFilePath) {
  std::vector<PSX::TmdMesh> meshes;
  if (!PSX::TmdLoader::LoadFromFile(tmdFilePath, meshes)) {
    std::fprintf(stderr, "[DX12] Failed to load TMD model file\\n");
    return false;
  }

  std::vector<PS1Vertex> allVerts;
  for (const auto &mesh : meshes) {
    allVerts.insert(allVerts.end(), mesh.vertices.begin(), mesh.vertices.end());
  }

  if (allVerts.empty()) {
    std::fprintf(stderr, "[DX12] TMD model contained 0 vertices\\n");
    return false;
  }

  m_TmdVertexCount = static_cast<uint32_t>(allVerts.size());
  const UINT vertexBufferSize =
      static_cast<UINT>(allVerts.size() * sizeof(PS1Vertex));

  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = vertexBufferSize;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  if (FAILED(m_Device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&m_TmdVertexBuffer)))) {
    std::fprintf(stderr, "[DX12] Failed to create TMD vertex buffer\\n");
    return false;
  }

  uint8_t *pVertexDataBegin = nullptr;
  D3D12_RANGE readRange = {0, 0};
  m_TmdVertexBuffer->Map(0, &readRange,
                         reinterpret_cast<void **>(&pVertexDataBegin));
  std::memcpy(pVertexDataBegin, allVerts.data(), vertexBufferSize);
  m_TmdVertexBuffer->Unmap(0, nullptr);

  m_TmdVertexBufferView.BufferLocation =
      m_TmdVertexBuffer->GetGPUVirtualAddress();
  m_TmdVertexBufferView.StrideInBytes = sizeof(PS1Vertex);
  m_TmdVertexBufferView.SizeInBytes = vertexBufferSize;

  std::printf("[DX12] Uploaded TMD mesh (%u vertices) to GPU\\n",
              m_TmdVertexCount);
  return true;
}

void DX12Renderer::RenderTmdMesh(float angleX, float angleY, float scale) {
  if (m_TmdVertexCount == 0 || !m_TmdVertexBuffer)
    return;

  m_Pipeline.Bind(m_CommandList.Get());

  XMMATRIX world = XMMatrixScaling(scale, scale, scale) *
                   XMMatrixRotationX(angleX) * XMMatrixRotationY(angleY) *
                   XMMatrixTranslation(0.0f, -0.1f, 0.0f);

  float jitterX = 0.0f, jitterY = 0.0f;
  if (m_DlssHook.GetCurrentMode() != UpscalerMode::Off) {
    GetHaltonJitter(m_TotalFrames, 8, jitterX, jitterY);
  }

  SceneConstants constants;
  constants.worldViewProj = XMMatrixTranspose(world);
  constants.prevWorldViewProj = XMMatrixTranspose(m_PrevWorldViewProj);
  constants.screenResolution =
      XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
  constants.blendMode = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
  constants.jitterOffset = XMFLOAT4(jitterX / 320.0f, jitterY / 240.0f, 0.0f, 0.0f);
  m_Pipeline.UpdateConstants(m_CommandList.Get(), constants);

  m_PrevWorldViewProj = world;

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  m_CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  m_CommandList->SetGraphicsRootDescriptorTable(
      1, m_SrvHeap->GetGPUDescriptorHandleForHeapStart());

  m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_CommandList->IASetVertexBuffers(0, 1, &m_TmdVertexBufferView);
  m_CommandList->DrawInstanced(m_TmdVertexCount, 1, 0, 0);
}

} // namespace Render
"""
    idx = text.rfind('} // namespace Render')
    if idx != -1:
        text = text[:idx] + methods
        print("Appended methods")

with open(path, 'wb') as f:
    f.write(text.encode('utf-8'))

print("Completed safe update of dx12_renderer.cpp")
