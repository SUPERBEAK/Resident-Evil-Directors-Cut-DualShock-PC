#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace Render {
class PS1Pipeline;
struct PS1Vertex;
} // namespace Render

namespace Game {

enum class InventoryAction { None, Equip, Use, Check, Combine };

struct InventorySlot {
  uint8_t itemId = 0;
  uint8_t count = 0;
};

class InventorySystem {
public:
  InventorySystem() = default;
  ~InventorySystem() = default;

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  const std::wstring &statusTimPath,
                  const std::wstring &itemAllPixPath);
  void Shutdown();

  void ToggleOpen();
  bool IsOpen() const { return m_IsOpen; }

  void Update(float deltaTime, uint32_t padButtons);
  void Render(ID3D12GraphicsCommandList *cmdList,
              Render::PS1Pipeline &pipeline);

  bool AddItem(uint8_t itemId, uint8_t count = 1);
  bool RemoveItem(uint8_t slotIdx, uint8_t count = 1);
  bool HasItem(uint8_t itemId) const {
    for (const auto &slot : m_Slots) {
      if (slot.itemId == itemId)
        return true;
    }
    return false;
  }
  const std::vector<InventorySlot> &GetSlots() const { return m_Slots; }
  std::vector<InventorySlot> &GetSlotsMutable() { return m_Slots; }
  void SetSlot(size_t index, uint8_t itemId, uint8_t count) {
    if (index < m_Slots.size()) {
      m_Slots[index].itemId = itemId;
      m_Slots[index].count = count;
    }
  }
  void SetEquippedSlot(uint8_t slot) { m_EquippedSlot = slot; }
  bool RemoveItemByItemId(uint8_t itemId, uint8_t count = 1);
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return m_SrvGpuHandle; }
  ID3D12DescriptorHeap *GetSrvHeap() const { return m_SrvHeap.Get(); }
  uint8_t GetEquippedSlot() const { return m_EquippedSlot; }
  uint8_t GetEquippedItemId() const {
    if (m_EquippedSlot < m_Slots.size()) {
      return m_Slots[m_EquippedSlot].itemId;
    }
    return 0;
  }

  uint8_t GetPendingUsedItemId() const { return m_PendingUsedItemId; }
  uint8_t GetPendingUsedSlotIdx() const { return m_PendingUsedSlotIdx; }
  void ClearPendingUsedItem() { m_PendingUsedItemId = 0; }

  void ShowPickupPopup(uint8_t itemId);
  bool HasActivePickupPopup() const { return m_PickupPopupTimer > 0.0f; }

  bool IsExamining() const { return m_Examining; }

  static const char *GetItemName(uint8_t itemId);
  static const char *GetItemDescription(uint8_t itemId);

private:
  void RebuildMenuVertices();
  void ExecuteAction(InventoryAction action);
  bool CombineItems(uint8_t srcSlot, uint8_t dstSlot);

  bool m_IsOpen = false;
  ID3D12Device *m_Device = nullptr;
  ID3D12CommandQueue *m_CmdQueue = nullptr;

  uint8_t m_MaxSlots = 6;
  uint8_t m_CursorSlot = 0;
  uint8_t m_EquippedSlot = 1;
  uint8_t m_CombineSrcSlot = 0xFF;

  bool m_SubmenuActive = false;
  uint8_t m_SubmenuCursor = 0;

  bool m_Examining = false;
  float m_ExamineAngle = 0.0f;

  uint8_t m_PickupPopupItemId = 0;
  float m_PickupPopupTimer = 0.0f;

  std::vector<InventorySlot> m_Slots;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_StatusTexture;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
  D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpuHandle = {};

  Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
  void *m_VertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
  uint32_t m_VertexCount = 0;

  std::vector<uint8_t> m_RawItemAllPix;
  std::vector<uint32_t> m_StatusClut2;

  uint8_t m_PendingUsedItemId = 0;
  uint8_t m_PendingUsedSlotIdx = 0;
};

} // namespace Game
