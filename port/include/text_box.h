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

class TextBox {
public:
  TextBox() = default;
  ~TextBox() = default;

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  const std::wstring &fontTimPath,
                  const std::wstring &slusPath);
  void Shutdown();

  bool IsActive() const { return m_IsActive; }

  void ShowMessage(const std::string &message, bool isPrompt = false,
                   std::function<void(bool)> callback = nullptr);
  void ShowRoomMessage(uint32_t messageIndex,
                       const std::vector<uint8_t> &rdtData,
                       bool isPrompt = false,
                       std::function<void(bool)> callback = nullptr);
  void Close();

  void Update(float deltaTime, uint32_t buttons);
  void Render(ID3D12GraphicsCommandList *cmdList,
              Render::PS1Pipeline &pipeline);

  static std::string DecodeCapcomString(const uint8_t *data, size_t maxLen,
                                       const std::string &itemInsert = "");

private:
  void RebuildVertices();
  void AdvancePage();

  bool m_IsActive = false;
  ID3D12Device *m_Device = nullptr;
  ID3D12CommandQueue *m_CmdQueue = nullptr;

  std::vector<std::string> m_Pages;
  size_t m_CurrentPage = 0;
  float m_TypewriterTimer = 0.0f;
  size_t m_CharsRevealed = 0;
  bool m_PageFullyRevealed = false;

  bool m_IsPrompt = false;
  int m_PromptSelection = 0; // 0 = Yes, 1 = No
  std::function<void(bool)> m_Callback = nullptr;

  float m_BlinkTimer = 0.0f;
  uint32_t m_PrevButtons = 0;
  bool m_PrevActionDown = false;
  bool m_PrevUpDown = false;
  bool m_PrevDownDown = false;

  std::vector<std::string> m_GlobalMessages;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_FontTexture;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
  D3D12_GPU_DESCRIPTOR_HANDLE m_SrvGpuHandle = {};

  Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
  void *m_VertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
  uint32_t m_VertexCount = 0;
};

extern TextBox g_TextBox;

} // namespace Game
