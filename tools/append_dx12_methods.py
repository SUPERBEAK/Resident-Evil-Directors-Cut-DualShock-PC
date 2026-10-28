methods = """
void DX12Renderer::RenderBackground() {
  m_Pipeline.Bind(m_CommandList.Get());

  SceneConstants constants;
  constants.worldViewProj = XMMatrixIdentity();
  constants.prevWorldViewProj = XMMatrixIdentity();
  constants.screenResolution =
      XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f); // Dither + 15-bit
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

  ID3D12PipelineState* pso = m_Pipeline.GetMotionVectorPipelineState();
  if (pso) {
    m_CommandList->SetGraphicsRootSignature(m_Pipeline.GetRootSignature());
    m_CommandList->SetPipelineState(pso);
  } else {
    m_Pipeline.Bind(m_CommandList.Get());
  }

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
  constants.renderFlags = XMFLOAT4(
      1.0f, 1.0f, 1.0f, 1.0f);
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

path = "port/src/renderer/dx12_renderer.cpp"
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

idx = text.rfind("} // namespace Render")
if idx != -1:
    new_text = text[:idx] + methods
    with open(path, "w", encoding="utf-8") as f:
        f.write(new_text)
    print("Successfully appended methods to dx12_renderer.cpp")
else:
    print("Could not find closing namespace brace")
