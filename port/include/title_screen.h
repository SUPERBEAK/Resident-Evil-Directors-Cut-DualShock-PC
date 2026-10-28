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

class TitleScreen {
public:
  enum class Phase {
    Inactive = 0,
    GoreWarning,
    CapcomLogo,
    TitleMenu,
    StartingGame
  };

  enum class MenuPage {
    PressAnyButton = 0,
    MainMenu,
    DifficultySelect
  };

  TitleScreen() = default;
  ~TitleScreen() = default;

  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *cmdQueue,
                  const std::wstring &dataDir);
  void Shutdown();

  void StartSequence(std::function<void()> onStartNewGame,
                     std::function<void()> onLoadGame = nullptr);
  void SkipToGameplay();

  bool IsActive() const { return m_Phase != Phase::Inactive; }
  Phase GetPhase() const { return m_Phase; }

  void Update(float deltaTime, uint32_t buttons);
  void Render(ID3D12GraphicsCommandList *cmdList,
              Render::PS1Pipeline &pipeline);

private:
  bool UploadTextureRGBA(const uint32_t *pixels, uint32_t width, uint32_t height,
                         Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
                         uint32_t descriptorIndex);
  bool LoadWarningTim(const std::wstring &path);
  bool LoadTitlePix(const std::wstring &path);
  bool LoadTmojiVariants(const std::wstring &path);

  Phase m_Phase = Phase::Inactive;
  MenuPage m_MenuPage = MenuPage::PressAnyButton;

  ID3D12Device *m_Device = nullptr;
  ID3D12CommandQueue *m_CmdQueue = nullptr;

  float m_Timer = 0.0f;
  float m_Brightness = 0.0f;
  int m_MainSelection = 0;       // 0 = New Game, 1 = Load Game, 2 = Option
  int m_DifficultySelection = 0; // 0 = Original, 1 = Beginner, 2 = Arrange
  bool m_SelectLoadGame = false;

  uint32_t m_PrevButtons = 0xFFFFFFFFu;
  bool m_PrevActionDown = true;
  bool m_PrevCancelDown = true;
  bool m_PrevUpDown = true;
  bool m_PrevDownDown = true;

  std::function<void()> m_OnStartNewGame = nullptr;
  std::function<void()> m_OnLoadGame = nullptr;

  // Descriptor heap slots:
  // 0: GoreWarning (320x240)
  // 1: TitlePix (320x240)
  // 2..8: TMOJI menu variants (256x80):
  //   2 = Press Any Button (TIM 0, CLUT 0)
  //   3 = Main Menu: New Game selected (TIM 1, CLUT 0)
  //   4 = Main Menu: Load Game selected (TIM 1, CLUT 1)
  //   5 = Main Menu: Option selected (TIM 1, CLUT 2)
  //   6 = Difficulty: Original (TIM 2, CLUT 0)
  //   7 = Difficulty: Beginner (TIM 3, CLUT 0)
  //   8 = Difficulty: Arrange (TIM 4, CLUT 0)
  static constexpr uint32_t kNumDescriptors = 9;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
  UINT m_DescriptorSize = 0;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_Textures[kNumDescriptors];

  Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
  void *m_VertexBufferMapped = nullptr;
  D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
};

extern TitleScreen g_TitleScreen;

} // namespace Game

