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
}

namespace Game {

class InventorySystem;

struct ItemBoxSlot {
  uint8_t itemId = 0;
  uint8_t count = 0;
};

class ItemBox {
public:
  ItemBox() = default;
  ~ItemBox() = default;

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  InventorySystem *inventory);
  void Shutdown();

  void Open();
  void Close();
  bool IsOpen() const { return m_IsOpen; }

  void Update(float deltaTime, uint32_t padButtons);
  void Render(ID3D12GraphicsCommandList *cmdList,
              Render::PS1Pipeline &pipeline);

  const std::vector<ItemBoxSlot> &GetBoxSlots() const { return m_BoxSlots; }
  std::vector<ItemBoxSlot> &GetBoxSlotsMutable() { return m_BoxSlots; }
  void SetBoxSlot(size_t index, uint8_t itemId, uint8_t count);
  bool AddItemToBox(uint8_t itemId, uint8_t count = 1);
  bool RemoveItemFromBox(size_t index, uint8_t count = 1);

private:
  void RebuildVertices();
  void StoreItem(size_t inventorySlotIdx);
  void TakeItem(size_t boxSlotIdx);

  bool m_IsOpen = false;
  ID3D12Device *m_Device = nullptr;
  ID3D12CommandQueue *m_CmdQueue = nullptr;
  InventorySystem *m_Inventory = nullptr;

  int m_ActiveColumn = 0;
  int m_InventoryCursor = 0;
  int m_BoxCursor = 0;
  int m_BoxScrollOffset = 0;

  bool m_SubmenuActive = false;
  int m_SubmenuSelection = 0;

  std::vector<ItemBoxSlot> m_BoxSlots;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
  void *m_VertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
  uint32_t m_VertexCount = 0;

  float m_BlinkTimer = 0.0f;
};

extern ItemBox g_ItemBox;

} // namespace Game
