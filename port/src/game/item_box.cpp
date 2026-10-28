#include "item_box.h"
#include "audio_engine.h"
#include "inventory_system.h"
#include "ps1_pipeline.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <windows.h>

using namespace DirectX;

namespace Game {

ItemBox g_ItemBox;

bool ItemBox::Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                         InventorySystem *inventory) {
  m_Device = device;
  m_CmdQueue = cmdQueue;
  m_Inventory = inventory;

  // Initialize 32 box slots
  m_BoxSlots.assign(32, {0, 0});

  // Authentic default RE1 starting storage chest items
  m_BoxSlots[0] = {65, 1};  // First-Aid Spray
  m_BoxSlots[1] = {47, 3};  // Ink Ribbon x3
  m_BoxSlots[2] = {12, 14}; // Shotgun Shells x14
  m_BoxSlots[3] = {11, 15}; // Handgun Clip x15

  constexpr uint32_t maxVertices = 2048;
  const uint32_t bufferSize = maxVertices * sizeof(Render::PS1Vertex);

  D3D12_HEAP_PROPERTIES uploadProps = {};
  uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = bufferSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  HRESULT hr = m_Device->CreateCommittedResource(
      &uploadProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&m_VertexBuffer));
  if (FAILED(hr)) {
    std::fprintf(stderr, "[ItemBox] Failed to create vertex buffer (0x%08X)\n", hr);
    return false;
  }

  D3D12_RANGE readRange = {0, 0};
  hr = m_VertexBuffer->Map(0, &readRange, &m_VertexBufferMapped);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[ItemBox] Failed to map vertex buffer (0x%08X)\n", hr);
    return false;
  }

  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Render::PS1Vertex);
  m_VertexBufferView.SizeInBytes = bufferSize;

  std::printf("[ItemBox] Initialized with 32 slots and starting survival supplies\n");
  return true;
}

void ItemBox::Shutdown() {
  if (m_VertexBuffer && m_VertexBufferMapped) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
  }
  m_VertexBuffer.Reset();
}

void ItemBox::Open() {
  m_IsOpen = true;
  m_ActiveColumn = 0;
  m_InventoryCursor = 0;
  m_BoxCursor = 0;
  m_BoxScrollOffset = 0;
  m_SubmenuActive = false;
  m_BlinkTimer = 0.0f;
  g_AudioEngine.PlayItemBoxOpen();
}

void ItemBox::Close() {
  m_IsOpen = false;
  m_SubmenuActive = false;
  g_AudioEngine.PlayItemBoxClose();
}

void ItemBox::SetBoxSlot(size_t index, uint8_t itemId, uint8_t count) {
  if (index < m_BoxSlots.size()) {
    m_BoxSlots[index].itemId = itemId;
    m_BoxSlots[index].count = count;
  }
}

bool ItemBox::AddItemToBox(uint8_t itemId, uint8_t count) {
  if (itemId == 0) return false;

  if (itemId == 11 || itemId == 12 || itemId == 14 || itemId == 47) {
    for (auto &slot : m_BoxSlots) {
      if (slot.itemId == itemId) {
        slot.count = static_cast<uint8_t>(std::min(250, slot.count + count));
        return true;
      }
    }
  }

  for (auto &slot : m_BoxSlots) {
    if (slot.itemId == 0) {
      slot.itemId = itemId;
      slot.count = count;
      return true;
    }
  }
  return false;
}

bool ItemBox::RemoveItemFromBox(size_t index, uint8_t count) {
  if (index >= m_BoxSlots.size() || m_BoxSlots[index].itemId == 0)
    return false;

  if (m_BoxSlots[index].count <= count) {
    m_BoxSlots[index] = {0, 0};
  } else {
    m_BoxSlots[index].count -= count;
  }
  return true;
}

void ItemBox::StoreItem(size_t inventorySlotIdx) {
  if (!m_Inventory) return;
  const auto &slots = m_Inventory->GetSlots();
  if (inventorySlotIdx >= slots.size() || slots[inventorySlotIdx].itemId == 0) {
    g_AudioEngine.PlayMenuCancel();
    return;
  }

  uint8_t id = slots[inventorySlotIdx].itemId;
  uint8_t cnt = slots[inventorySlotIdx].count;

  if (AddItemToBox(id, cnt)) {
    m_Inventory->RemoveItem(static_cast<uint8_t>(inventorySlotIdx), cnt);
    g_AudioEngine.PlayMenuConfirm();
  } else {
    g_AudioEngine.PlayMenuCancel();
  }
}

void ItemBox::TakeItem(size_t boxSlotIdx) {
  if (!m_Inventory) return;
  if (boxSlotIdx >= m_BoxSlots.size() || m_BoxSlots[boxSlotIdx].itemId == 0) {
    g_AudioEngine.PlayMenuCancel();
    return;
  }

  uint8_t id = m_BoxSlots[boxSlotIdx].itemId;
  uint8_t cnt = m_BoxSlots[boxSlotIdx].count;

  if (m_Inventory->AddItem(id, cnt)) {
    RemoveItemFromBox(boxSlotIdx, cnt);
    g_AudioEngine.PlayMenuConfirm();
  } else {
    g_AudioEngine.PlayMenuCancel();
  }
}

void ItemBox::Update(float deltaTime, uint32_t padButtons) {
  if (!m_IsOpen) return;
  m_BlinkTimer += deltaTime;

  static bool s_PrevUp = false, s_PrevDown = false, s_PrevLeft = false,
              s_PrevRight = false, s_PrevConfirm = false, s_PrevCancel = false;

  bool upRaw = ((GetAsyncKeyState(VK_UP) & 0x8000) != 0) ||
               ((GetAsyncKeyState('W') & 0x8000) != 0) ||
               ((padButtons & (1 << 4)) != 0);
  bool downRaw = ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) ||
                 ((GetAsyncKeyState('S') & 0x8000) != 0) ||
                 ((padButtons & (1 << 5)) != 0);
  bool leftRaw = ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) ||
                 ((GetAsyncKeyState('A') & 0x8000) != 0) ||
                 ((padButtons & (1 << 6)) != 0);
  bool rightRaw = ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) ||
                  ((GetAsyncKeyState('D') & 0x8000) != 0) ||
                  ((padButtons & (1 << 7)) != 0);
  bool confirmRaw = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ||
                    ((GetAsyncKeyState('Z') & 0x8000) != 0) ||
                    ((GetAsyncKeyState('E') & 0x8000) != 0) ||
                    ((padButtons & (1 << 0)) != 0) ||
                    ((padButtons & (1 << 2)) != 0);
  bool cancelRaw = ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) ||
                   ((GetAsyncKeyState(VK_BACK) & 0x8000) != 0) ||
                   ((GetAsyncKeyState('X') & 0x8000) != 0) ||
                   ((padButtons & (1 << 1)) != 0);

  bool up = (upRaw && !s_PrevUp);
  bool down = (downRaw && !s_PrevDown);
  bool left = (leftRaw && !s_PrevLeft);
  bool right = (rightRaw && !s_PrevRight);
  bool confirm = (confirmRaw && !s_PrevConfirm);
  bool cancel = (cancelRaw && !s_PrevCancel);

  s_PrevUp = upRaw;
  s_PrevDown = downRaw;
  s_PrevLeft = leftRaw;
  s_PrevRight = rightRaw;
  s_PrevConfirm = confirmRaw;
  s_PrevCancel = cancelRaw;

  if (cancel) {
    if (m_SubmenuActive) {
      m_SubmenuActive = false;
      g_AudioEngine.PlayMenuCancel();
    } else {
      Close();
    }
    return;
  }

  if (m_SubmenuActive) {
    if (up || down) {
      m_SubmenuSelection = 1 - m_SubmenuSelection;
      g_AudioEngine.PlayMenuCursor();
    }
    if (confirm) {
      if (m_SubmenuSelection == 1) {
        m_SubmenuActive = false;
        g_AudioEngine.PlayMenuCancel();
      } else {
        if (m_ActiveColumn == 0) {
          StoreItem(m_InventoryCursor);
        } else {
          TakeItem(m_BoxCursor);
        }
        m_SubmenuActive = false;
      }
    }
    return;
  }

  // Column switching
  if (left && m_ActiveColumn == 1) {
    m_ActiveColumn = 0;
    g_AudioEngine.PlayMenuCursor();
  } else if (right && m_ActiveColumn == 0) {
    m_ActiveColumn = 1;
    g_AudioEngine.PlayMenuCursor();
  }

  // Row navigation
  if (m_ActiveColumn == 0) {
    if (up && m_InventoryCursor > 0) {
      m_InventoryCursor--;
      g_AudioEngine.PlayMenuCursor();
    } else if (down && m_InventoryCursor < 5) {
      m_InventoryCursor++;
      g_AudioEngine.PlayMenuCursor();
    }
    if (confirm) {
      const auto &slots = m_Inventory->GetSlots();
      if (m_InventoryCursor < slots.size() && slots[m_InventoryCursor].itemId != 0) {
        m_SubmenuActive = true;
        m_SubmenuSelection = 0;
        g_AudioEngine.PlayMenuConfirm();
      } else {
        g_AudioEngine.PlayMenuCancel();
      }
    }
  } else {
    if (up && m_BoxCursor > 0) {
      m_BoxCursor--;
      if (m_BoxCursor < m_BoxScrollOffset) {
        m_BoxScrollOffset = m_BoxCursor;
      }
      g_AudioEngine.PlayMenuCursor();
    } else if (down && m_BoxCursor < 31) {
      m_BoxCursor++;
      if (m_BoxCursor >= m_BoxScrollOffset + 6) {
        m_BoxScrollOffset = m_BoxCursor - 5;
      }
      g_AudioEngine.PlayMenuCursor();
    }
    if (confirm) {
      if (m_BoxCursor < m_BoxSlots.size() && m_BoxSlots[m_BoxCursor].itemId != 0) {
        m_SubmenuActive = true;
        m_SubmenuSelection = 0;
        g_AudioEngine.PlayMenuConfirm();
      } else {
        g_AudioEngine.PlayMenuCancel();
      }
    }
  }
}

void ItemBox::RebuildVertices() {
  if (!m_VertexBufferMapped) return;
  std::vector<Render::PS1Vertex> verts;
  verts.reserve(2048);

  auto AddQuad = [&](float x, float y, float w, float h, float u0, float v0,
                     float u1, float v1, XMFLOAT4 col = {1, 1, 1, 1}) {
    float ndcX0 = (x / 320.0f) * 2.0f - 1.0f;
    float ndcX1 = ((x + w) / 320.0f) * 2.0f - 1.0f;
    float ndcY0 = 1.0f - (y / 240.0f) * 2.0f;
    float ndcY1 = 1.0f - ((y + h) / 240.0f) * 2.0f;
    float uvX0 = u0 / 512.0f;
    float uvX1 = u1 / 512.0f;
    float uvY0 = v0 / 512.0f;
    float uvY1 = v1 / 512.0f;
    Render::PS1Vertex q0{{ndcX0, ndcY0, 0.0f}, col, {uvX0, uvY0}};
    Render::PS1Vertex q1{{ndcX1, ndcY0, 0.0f}, col, {uvX1, uvY0}};
    Render::PS1Vertex q2{{ndcX0, ndcY1, 0.0f}, col, {uvX0, uvY1}};
    Render::PS1Vertex q3{{ndcX1, ndcY1, 0.0f}, col, {uvX1, uvY1}};
    verts.push_back(q0); verts.push_back(q1); verts.push_back(q2);
    verts.push_back(q1); verts.push_back(q3); verts.push_back(q2);
  };

  auto AddSolidQuad = [&](float x, float y, float w, float h, XMFLOAT4 col) {
    AddQuad(x, y, w, h, 511.0f, 511.0f, 511.5f, 511.5f, col);
  };

  auto DrawText = [&](float tx, float ty, const char *str, XMFLOAT4 col = {1, 1, 1, 1}) {
    float cx = tx;
    for (const char *p = str; *p; ++p) {
      char ch = *p;
      if (ch == '\n') {
        cx = tx;
        ty += 10.0f;
        continue;
      }
      if (ch >= 32 && ch <= 126) {
        int gIdx = ch - 32;
        int cCol = gIdx % 16;
        int cRow = gIdx / 16;
        float gx = static_cast<float>(cCol * 8);
        float gy = static_cast<float>(256 + cRow * 8);
        AddQuad(cx, ty, 8.0f, 8.0f, gx, gy, gx + 8.0f, gy + 8.0f, col);
      }
      cx += 8.0f;
    }
  };

  auto DrawWrappedText = [&](float tx, float ty, const char *str, int maxCharsPerLine, XMFLOAT4 col = {1, 1, 1, 1}) {
    float cx = tx;
    float cy = ty;
    int lineCharCount = 0;
    const char *p = str;
    while (*p) {
      const char *wordStart = p;
      while (*p && *p != ' ' && *p != '\n') p++;
      int wordLen = static_cast<int>(p - wordStart);
      if (lineCharCount + wordLen > maxCharsPerLine && lineCharCount > 0) {
        cx = tx;
        cy += 10.0f;
        lineCharCount = 0;
      }
      for (int i = 0; i < wordLen; ++i) {
        char ch = wordStart[i];
        if (ch >= 32 && ch <= 126) {
          int gIdx = ch - 32;
          int cCol = gIdx % 16;
          int cRow = gIdx / 16;
          float gx = static_cast<float>(cCol * 8);
          float gy = static_cast<float>(256 + cRow * 8);
          AddQuad(cx, cy, 8.0f, 8.0f, gx, gy, gx + 8.0f, gy + 8.0f, col);
        }
        cx += 8.0f;
        lineCharCount++;
      }
      if (*p == ' ') {
        cx += 8.0f;
        lineCharCount++;
        p++;
      } else if (*p == '\n') {
        cx = tx;
        cy += 10.0f;
        lineCharCount = 0;
        p++;
      }
    }
  };

  bool blinkOn = (std::fmod(m_BlinkTimer, 0.4f) < 0.25f);

  // 1. Fullscreen translucent backdrop
  AddSolidQuad(0, 0, 320, 240, {0.02f, 0.04f, 0.08f, 0.94f});

  // 2. Top Title Bar
  AddSolidQuad(10, 8, 300, 18, {0.06f, 0.12f, 0.32f, 0.95f});
  AddSolidQuad(10, 8, 300, 1, {0.35f, 0.55f, 0.85f, 1.0f});
  AddSolidQuad(10, 25, 300, 1, {0.35f, 0.55f, 0.85f, 1.0f});
  DrawText(88, 13, "ITEM STORAGE CHEST", {1.0f, 0.92f, 0.25f, 1.0f});

  // 3. Left Panel: CHRIS INVENTORY (6 slots)
  float invX = 10.0f, invY = 30.0f, invW = 146.0f, invH = 136.0f;
  AddSolidQuad(invX - 1, invY - 1, invW + 2, invH + 2, (m_ActiveColumn == 0) ? XMFLOAT4{0.35f, 0.55f, 0.85f, 1.0f} : XMFLOAT4{0.15f, 0.22f, 0.40f, 0.8f});
  AddSolidQuad(invX, invY, invW, invH, {0.04f, 0.08f, 0.22f, 0.95f});

  AddSolidQuad(invX, invY, invW, 14, (m_ActiveColumn == 0) ? XMFLOAT4{0.10f, 0.22f, 0.55f, 1.0f} : XMFLOAT4{0.06f, 0.12f, 0.30f, 0.9f});
  DrawText(invX + 16, invY + 3, "INVENTORY (6)", (m_ActiveColumn == 0) ? XMFLOAT4{1.0f, 0.95f, 0.2f, 1.0f} : XMFLOAT4{0.7f, 0.8f, 0.9f, 1.0f});

  const auto &invSlots = m_Inventory ? m_Inventory->GetSlots() : std::vector<InventorySlot>{};
  for (int i = 0; i < 6; ++i) {
    float rowY = invY + 16.0f + i * 20.0f;
    bool isSelected = (m_ActiveColumn == 0 && m_InventoryCursor == i);

    if (isSelected) {
      AddSolidQuad(invX + 2, rowY, invW - 4, 18, {0.18f, 0.34f, 0.70f, 0.95f});
      if (blinkOn) {
        DrawText(invX + 4, rowY + 5, ">", {1.0f, 0.95f, 0.2f, 1.0f});
      }
    } else {
      AddSolidQuad(invX + 2, rowY, invW - 4, 18, {0.06f, 0.10f, 0.25f, 0.6f});
    }

    if (i < invSlots.size() && invSlots[i].itemId != 0) {
      uint8_t itemId = invSlots[i].itemId;
      uint8_t count = invSlots[i].count;

      if (itemId < 77) {
        uint8_t iconIdx = itemId - 1;
        uint32_t col = iconIdx % 6;
        uint32_t row = iconIdx / 6;
        float u0 = 256.0f + col * 42.0f;
        float v0 = row * 32.0f;
        AddQuad(invX + 14, rowY + 2, 18, 14, u0, v0, u0 + 40.0f, v0 + 30.0f);
      }

      char nameBuf[16] = {};
      const char *fullName = InventorySystem::GetItemName(itemId);
      std::strncpy(nameBuf, fullName, 10);
      DrawText(invX + 36, rowY + 5, nameBuf, isSelected ? XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f} : XMFLOAT4{0.85f, 0.88f, 0.92f, 1.0f});

      if (count > 1 || itemId == 2 || itemId == 3 || itemId == 5 || itemId == 11 || itemId == 12 || itemId == 14 || itemId == 47) {
        char cntStr[8];
        std::snprintf(cntStr, sizeof(cntStr), "%d", count);
        DrawText(invX + 120, rowY + 5, cntStr, {0.4f, 0.85f, 1.0f, 1.0f});
      }
    } else {
      DrawText(invX + 36, rowY + 5, "[EMPTY]", {0.35f, 0.45f, 0.55f, 0.8f});
    }
  }

  // 4. Right Panel: ITEM BOX (32 slots)
  float boxX = 164.0f, boxY = 30.0f, boxW = 146.0f, boxH = 136.0f;
  AddSolidQuad(boxX - 1, boxY - 1, boxW + 2, boxH + 2, (m_ActiveColumn == 1) ? XMFLOAT4{0.35f, 0.55f, 0.85f, 1.0f} : XMFLOAT4{0.15f, 0.22f, 0.40f, 0.8f});
  AddSolidQuad(boxX, boxY, boxW, boxH, {0.04f, 0.08f, 0.22f, 0.95f});

  AddSolidQuad(boxX, boxY, boxW, 14, (m_ActiveColumn == 1) ? XMFLOAT4{0.10f, 0.22f, 0.55f, 1.0f} : XMFLOAT4{0.06f, 0.12f, 0.30f, 0.9f});
  DrawText(boxX + 16, boxY + 3, "ITEM BOX (32)", (m_ActiveColumn == 1) ? XMFLOAT4{1.0f, 0.95f, 0.2f, 1.0f} : XMFLOAT4{0.7f, 0.8f, 0.9f, 1.0f});

  if (m_BoxScrollOffset > 0) {
    DrawText(boxX + 132, boxY + 3, "^", {1.0f, 0.92f, 0.25f, 1.0f});
  }
  if (m_BoxScrollOffset + 6 < 32) {
    DrawText(boxX + 132, boxY + 124, "v", {1.0f, 0.92f, 0.25f, 1.0f});
  }

  for (int r = 0; r < 6; ++r) {
    int slotIdx = m_BoxScrollOffset + r;
    if (slotIdx >= 32) break;

    float rowY = boxY + 16.0f + r * 20.0f;
    bool isSelected = (m_ActiveColumn == 1 && m_BoxCursor == slotIdx);

    if (isSelected) {
      AddSolidQuad(boxX + 2, rowY, boxW - 4, 18, {0.18f, 0.34f, 0.70f, 0.95f});
      if (blinkOn) {
        DrawText(boxX + 4, rowY + 5, ">", {1.0f, 0.95f, 0.2f, 1.0f});
      }
    } else {
      AddSolidQuad(boxX + 2, rowY, boxW - 4, 18, {0.06f, 0.10f, 0.25f, 0.6f});
    }

    if (m_BoxSlots[slotIdx].itemId != 0) {
      uint8_t itemId = m_BoxSlots[slotIdx].itemId;
      uint8_t count = m_BoxSlots[slotIdx].count;

      if (itemId < 77) {
        uint8_t iconIdx = itemId - 1;
        uint32_t col = iconIdx % 6;
        uint32_t row = iconIdx / 6;
        float u0 = 256.0f + col * 42.0f;
        float v0 = row * 32.0f;
        AddQuad(boxX + 14, rowY + 2, 18, 14, u0, v0, u0 + 40.0f, v0 + 30.0f);
      }

      char nameBuf[16] = {};
      const char *fullName = InventorySystem::GetItemName(itemId);
      std::strncpy(nameBuf, fullName, 10);
      DrawText(boxX + 36, rowY + 5, nameBuf, isSelected ? XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f} : XMFLOAT4{0.85f, 0.88f, 0.92f, 1.0f});

      if (count > 1 || itemId == 2 || itemId == 3 || itemId == 5 || itemId == 11 || itemId == 12 || itemId == 14 || itemId == 47) {
        char cntStr[8];
        std::snprintf(cntStr, sizeof(cntStr), "%d", count);
        DrawText(boxX + 120, rowY + 5, cntStr, {0.4f, 0.85f, 1.0f, 1.0f});
      }
    } else {
      DrawText(boxX + 36, rowY + 5, "[EMPTY]", {0.35f, 0.45f, 0.55f, 0.8f});
    }
  }

  // 5. Bottom Info & Controls Panel
  float infoX = 10.0f, infoY = 172.0f, infoW = 300.0f, infoH = 60.0f;
  AddSolidQuad(infoX - 1, infoY - 1, infoW + 2, infoH + 2, {0.25f, 0.35f, 0.65f, 1.0f});
  AddSolidQuad(infoX, infoY, infoW, infoH, {0.035f, 0.07f, 0.22f, 0.98f});

  uint8_t activeItemId = 0;
  if (m_ActiveColumn == 0 && m_InventoryCursor < invSlots.size()) {
    activeItemId = invSlots[m_InventoryCursor].itemId;
  } else if (m_ActiveColumn == 1 && m_BoxCursor < m_BoxSlots.size()) {
    activeItemId = m_BoxSlots[m_BoxCursor].itemId;
  }

  if (activeItemId > 0 && activeItemId < 77) {
    DrawText(infoX + 8, infoY + 6, InventorySystem::GetItemName(activeItemId), {1.0f, 0.95f, 0.3f, 1.0f});
    DrawWrappedText(infoX + 8, infoY + 20, InventorySystem::GetItemDescription(activeItemId), 35, {0.88f, 0.92f, 0.96f, 1.0f});
  } else {
    DrawText(infoX + 8, infoY + 6, "EMPTY SLOT", {0.6f, 0.65f, 0.7f, 1.0f});
    DrawText(infoX + 8, infoY + 20, "Select an item to store or take.", {0.6f, 0.65f, 0.7f, 1.0f});
  }

  DrawText(infoX + 6, infoY + 46, "[<][>] COLUMN  [^][v] SELECT  [ENTER] ACTION  [ESC] CLOSE", {0.45f, 0.75f, 1.0f, 0.9f});

  // 6. Action Submenu Popup
  if (m_SubmenuActive) {
    float subX = 110.0f, subY = 82.0f, subW = 100.0f, subH = 56.0f;
    AddSolidQuad(subX - 2, subY - 2, subW + 4, subH + 4, {0.85f, 0.75f, 0.25f, 1.0f});
    AddSolidQuad(subX - 1, subY - 1, subW + 2, subH + 2, {0.04f, 0.08f, 0.20f, 1.0f});
    AddSolidQuad(subX, subY, subW, subH, {0.06f, 0.12f, 0.30f, 0.98f});

    AddSolidQuad(subX, subY, subW, 14, {0.12f, 0.22f, 0.50f, 1.0f});
    DrawText(subX + 26, subY + 3, "COMMAND", {1.0f, 0.95f, 0.3f, 1.0f});

    const char *actionText = (m_ActiveColumn == 0) ? "STORE" : "TAKE";
    if (m_SubmenuSelection == 0) {
      AddSolidQuad(subX + 4, subY + 18, subW - 8, 16, {0.20f, 0.38f, 0.75f, 1.0f});
      DrawText(subX + 8, subY + 22, ">", {1.0f, 0.95f, 0.2f, 1.0f});
      DrawText(subX + 20, subY + 22, actionText, {1.0f, 1.0f, 1.0f, 1.0f});
    } else {
      DrawText(subX + 20, subY + 22, actionText, {0.8f, 0.85f, 0.9f, 1.0f});
    }

    if (m_SubmenuSelection == 1) {
      AddSolidQuad(subX + 4, subY + 36, subW - 8, 16, {0.20f, 0.38f, 0.75f, 1.0f});
      DrawText(subX + 8, subY + 40, ">", {1.0f, 0.95f, 0.2f, 1.0f});
      DrawText(subX + 20, subY + 40, "CANCEL", {1.0f, 1.0f, 1.0f, 1.0f});
    } else {
      DrawText(subX + 20, subY + 40, "CANCEL", {0.8f, 0.85f, 0.9f, 1.0f});
    }
  }

  m_VertexCount = static_cast<uint32_t>(verts.size());
  if (m_VertexCount > 0 && m_VertexBufferMapped) {
    std::memcpy(m_VertexBufferMapped, verts.data(), m_VertexCount * sizeof(Render::PS1Vertex));
  }
}

void ItemBox::Render(ID3D12GraphicsCommandList *cmdList,
                     Render::PS1Pipeline &pipeline) {
  if (!m_IsOpen || !m_Inventory) return;

  RebuildVertices();
  if (m_VertexCount == 0) return;

  pipeline.Bind(cmdList);
  Render::SceneConstants constants;
  constants.worldViewProj = DirectX::XMMatrixIdentity();
  constants.prevWorldViewProj = DirectX::XMMatrixIdentity();
  constants.screenResolution =
      DirectX::XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
  constants.blendMode = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
  constants.jitterOffset = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  pipeline.UpdateConstants(cmdList, constants);

  ID3D12DescriptorHeap *srvHeap = m_Inventory->GetSrvHeap();
  if (srvHeap) {
    ID3D12DescriptorHeap *ppHeaps[] = {srvHeap};
    cmdList->SetDescriptorHeaps(1, ppHeaps);
    cmdList->SetGraphicsRootDescriptorTable(1, m_Inventory->GetSrvGpuHandle());
  }

  cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(m_VertexCount, 1, 0, 0);
}

} // namespace Game
