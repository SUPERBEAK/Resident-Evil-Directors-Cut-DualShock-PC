with open('port/src/hal/psx_str.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target1 = """    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = 1;"""

replace1 = """    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = 2;"""

assert target1 in text, "target1 not found"
text = text.replace(target1, replace1, 1)

target2 = """    m_Device->CreateShaderResourceView(m_MovieTexture.Get(), &srvViewDesc, m_SrvHeap->GetCPUDescriptorHandleForHeapStart());"""

replace2 = """    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT incSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_Device->CreateShaderResourceView(m_MovieTexture.Get(), &srvViewDesc, cpuHandle);
    cpuHandle.ptr += incSize;
    m_Device->CreateShaderResourceView(m_MovieTexture.Get(), &srvViewDesc, cpuHandle);"""

assert target2 in text, "target2 not found"
text = text.replace(target2, replace2, 1)

target3 = """    constants.renderFlags = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    constants.blendMode = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);"""

replace3 = """    constants.renderFlags = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    constants.blendMode = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    constants.jitterOffset = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);"""

assert target3 in text, "target3 not found"
text = text.replace(target3, replace3, 1)

with open('port/src/hal/psx_str.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("psx_str.cpp updated with 2 SRVs and matching renderFlags successfully.")
