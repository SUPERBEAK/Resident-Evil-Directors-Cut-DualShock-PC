#include "inventory_system.h"
#include "audio_engine.h"
#include "ps1_pipeline.h"
#include "psx_pad.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <windows.h>

namespace Game {

static const char *s_ItemNames[77] = {
    "Nothing",         "Combat Knife",      "Beretta",
    "Shotgun",         "Beretta (Special)", "Colt Python",
    "Flamethrower",    "Bazooka (Acid)",    "Bazooka (Explosive)",
    "Bazooka (Flame)", "Rocket Launcher",   "Handgun Clip",
    "Shotgun Shells",  "Lockpick",          "Magnum Rounds",
    "Fuel Bottle",     "Explosive Rounds",  "Acid Rounds",
    "Flame Rounds",    "Empty Bottle",      "Water",
    "Umb No. 3",       "Umb No. 4",         "Umb No. 7",
    "Umb No. 13",      "Yellow-6",          "NP-004",
    "V-JOLT",          "Broken Shotgun",    "Square Crank",
    "Hex Crank",       "Wood Emblem",       "Gold Emblem",
    "Blue Jewel",      "Red Jewel",         "Music Notes",
    "Wolf Medal",      "Eagle Medal",       "Chemical (Herbicide)",
    "Battery",         "MO Disk",           "Wind Crest",
    "Flare Gun",       "Slide Film",        "Moon Crest",
    "Star Crest",      "Sun Crest",         "Ink Ribbon",
    "Lighter",         "Moon Crest 2",      "Moon Crest 3",
    "Sword Key",       "Armor Key",         "Shield Key",
    "Helmet Key",      "Master Key",        "Special Key",
    "Dorm Key 002",    "Dorm Key 003",      "Control Room Key",
    "Power Room Key",  "Desk Key",          "Red Book",
    "Doom Book 1",     "Doom Book 2",       "First-Aid Spray",
    "Serum",           "Red Herb",          "Green Herb",
    "Blue Herb",       "Mixed (G+G)",       "Mixed (G+R)",
    "Mixed (G+B)",     "Mixed (G+G+G)",     "Mixed (G+G+B)",
    "Mixed (G+R+B)",   "Com. Radio"};

static const char *s_ItemDescriptions[77] = {
    "",
    "A sharp combat knife. Short range.",
    "9mm automatic pistol. Holds 15 rounds.",
    "12-gauge pump-action shotgun.",
    "Custom tuned 9mm automatic handgun.",
    ".357 Magnum revolver. High power.",
    "Custom chemical incendiary weapon.",
    "Grenade launcher loaded with acid rounds.",
    "Grenade launcher loaded with explosive rounds.",
    "Grenade launcher loaded with flame rounds.",
    "Four-barrel anti-tank rocket launcher.",
    "9mm Parabellum ammunition for Beretta.",
    "12-gauge buckshot ammunition.",
    "Used to pick simple cylinder locks.",
    ".357 magnum ammunition for Colt Python.",
    "Bottle containing liquid fuel.",
    "Explosive shells for grenade launcher.",
    "Acid chemical shells for grenade launcher.",
    "Napalm flame shells for grenade launcher.",
    "An empty laboratory beaker bottle.",
    "Distilled laboratory water.",
    "Umbrella chemical compound No. 3.",
    "Umbrella chemical compound No. 4.",
    "Umbrella chemical compound No. 7.",
    "Umbrella chemical compound No. 13.",
    "Yellow chemical compound.",
    "Chemical reagent NP-004.",
    "Chemical herbicide against Plant 42.",
    "A broken shotgun. Cannot be fired.",
    "A square-socket mechanical crank.",
    "A hexagonal-socket mechanical crank.",
    "Emblem carved from mahogany wood.",
    "Heavy emblem cast from solid gold.",
    "A brilliant cut blue sapphire gem.",
    "A brilliant cut crimson ruby gem.",
    "Sheet music: Moonlight Sonata.",
    "Bronze medal with wolf engraving.",
    "Gold medal with eagle engraving.",
    "Weed-killer chemical solution.",
    "Heavy lead-acid storage battery.",
    "Encrypted optical security disk.",
    "Stone crest with wind symbol.",
    "Emergency signaling flare gun.",
    "35mm photographic slide film.",
    "Stone crest with moon symbol.",
    "Stone crest with star symbol.",
    "Stone crest with sun symbol.",
    "Ribbon for manual typewriter.",
    "Silver flip-top fluid lighter.",
    "Second piece of the moon crest.",
    "Third piece of the moon crest.",
    "Mansion key with a sword symbol.",
    "Mansion key with an armor symbol.",
    "Mansion key with a shield symbol.",
    "Mansion key with a helmet symbol.",
    "Laboratory master electronic key.",
    "Key to mansion wardrobe closet.",
    "Key to dormitory room 002.",
    "Key to dormitory room 003.",
    "Key to aqua ring control room.",
    "Key to power generator room.",
    "Key to locked study desk drawer.",
    "Book titled: Destruction of Plants.",
    "Ancient tome concealing a medal.",
    "Ancient tome concealing a medal.",
    "Completely restores all health.",
    "Antidote for snake venom.",
    "Mix with other herbs for effect.",
    "Wild herb. Partially heals vitality.",
    "Wild herb. Cures poison.",
    "Double green herb. Large heal.",
    "Green and red herb. Full heal.",
    "Green and blue herb. Heals and cures.",
    "Triple green herb. Full heal.",
    "Two green and blue. Large heal and cures.",
    "Botanical mix. Full heal and cures.",
    "S.T.A.R.S. communication radio."};

static const uint8_t s_Font8x8[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
    {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00},
    {0x18, 0x7E, 0xC0, 0x7C, 0x06, 0x7E, 0x18, 0x00},
    {0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00},
    {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00},
    {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00},
    {0x3C, 0x66, 0xC3, 0xC3, 0xC3, 0x66, 0x3C, 0x00},
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    {0x7C, 0xC6, 0x06, 0x1C, 0x30, 0x60, 0xFE, 0x00},
    {0x7E, 0x0C, 0x18, 0x3C, 0x06, 0xC6, 0x7C, 0x00},
    {0x0C, 0x1C, 0x34, 0x64, 0xFE, 0x04, 0x04, 0x00},
    {0xFE, 0xC0, 0xFC, 0x06, 0x06, 0xC6, 0x7C, 0x00},
    {0x38, 0x60, 0xC0, 0xFC, 0xC6, 0xC6, 0x7C, 0x00},
    {0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
    {0x7C, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0x7C, 0x00},
    {0x7C, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0x78, 0x00},
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00},
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00},
    {0x7C, 0xC6, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x00},
    {0x7C, 0xC6, 0xDE, 0xDE, 0xDC, 0xC0, 0x7C, 0x00},
    {0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0x00},
    {0xFC, 0x66, 0x66, 0x7C, 0x66, 0x66, 0xFC, 0x00},
    {0x3C, 0x66, 0xC0, 0xC0, 0xC0, 0x66, 0x3C, 0x00},
    {0xF8, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00},
    {0xFE, 0x62, 0x68, 0x78, 0x68, 0x62, 0xFE, 0x00},
    {0xFE, 0x62, 0x68, 0x78, 0x68, 0x60, 0xF0, 0x00},
    {0x3C, 0x66, 0xC0, 0xC0, 0xCE, 0x66, 0x3E, 0x00},
    {0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00},
    {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    {0x1E, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00},
    {0xE6, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0xE6, 0x00},
    {0xF0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00},
    {0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00},
    {0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00},
    {0x38, 0x6C, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00},
    {0xFC, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00},
    {0x38, 0x6C, 0xC6, 0xC6, 0xD6, 0x6C, 0x3A, 0x00},
    {0xFC, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0xE6, 0x00},
    {0x7C, 0xC6, 0x60, 0x38, 0x0C, 0xC6, 0x7C, 0x00},
    {0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    {0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00},
    {0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00},
    {0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00},
    {0xC6, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0xC6, 0x00},
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x00},
    {0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x62, 0xFE, 0x00},
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
    {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00},
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
    {0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00},
    {0xE0, 0x60, 0x7C, 0x66, 0x66, 0x66, 0xDC, 0x00},
    {0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC6, 0x7C, 0x00},
    {0x1C, 0x0C, 0x7C, 0xCC, 0xCC, 0xCC, 0x76, 0x00},
    {0x00, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0x7C, 0x00},
    {0x1C, 0x36, 0x30, 0x78, 0x30, 0x30, 0x78, 0x00},
    {0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8},
    {0xE0, 0x60, 0x6C, 0x76, 0x66, 0x66, 0xE6, 0x00},
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    {0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C},
    {0xE0, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0xE6, 0x00},
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    {0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xD6, 0x00},
    {0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x00},
    {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x00},
    {0x00, 0x00, 0xDC, 0x66, 0x66, 0x7C, 0x60, 0xF0},
    {0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0x1E},
    {0x00, 0x00, 0xDC, 0x76, 0x60, 0x60, 0xF0, 0x00},
    {0x00, 0x00, 0x7E, 0xC0, 0x7C, 0x06, 0xFC, 0x00},
    {0x30, 0x30, 0x7C, 0x30, 0x30, 0x34, 0x18, 0x00},
    {0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00},
    {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00},
    {0x00, 0x00, 0xC6, 0xD6, 0xFE, 0xEE, 0x6C, 0x00},
    {0x00, 0x00, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x00},
    {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0xFC},
    {0x00, 0x00, 0xFE, 0x8C, 0x18, 0x32, 0xFE, 0x00},
    {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00},
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},
    {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00},
    {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

const char *InventorySystem::GetItemName(uint8_t itemId) {
  if (itemId < 77)
    return s_ItemNames[itemId];
  return "Unknown Item";
}

const char *InventorySystem::GetItemDescription(uint8_t itemId) {
  if (itemId < 77)
    return s_ItemDescriptions[itemId];
  return "";
}

void InventorySystem::ShowPickupPopup(uint8_t itemId) {
  m_PickupPopupItemId = itemId;
  m_PickupPopupTimer = 2.5f;
}

bool InventorySystem::Initialize(ID3D12Device *device,
                                 ID3D12CommandQueue *cmdQueue,
                                 const std::wstring &statusTimPath,
                                 const std::wstring &itemAllPixPath) {
  m_Device = device;
  m_CmdQueue = cmdQueue;
  std::ifstream timFile(statusTimPath, std::ios::binary | std::ios::ate);
  if (!timFile.is_open())
    return false;
  size_t timSize = static_cast<size_t>(timFile.tellg());
  timFile.seekg(0, std::ios::beg);
  std::vector<uint8_t> timData(timSize);
  timFile.read(reinterpret_cast<char *>(timData.data()), timSize);
  uint32_t clutBsize = *reinterpret_cast<const uint32_t *>(timData.data() + 8);
  uint16_t cnum = *reinterpret_cast<const uint16_t *>(timData.data() + 16);
  m_StatusClut2.resize(256);
  const uint16_t *pal2Raw =
      reinterpret_cast<const uint16_t *>(timData.data() + 20 + 2 * (cnum * 2));
  for (int i = 0; i < 256; ++i) {
    uint16_t c = pal2Raw[i];
    uint32_t r = ((c & 0x1F) * 255) / 31;
    uint32_t g = (((c >> 5) & 0x1F) * 255) / 31;
    uint32_t b = (((c >> 10) & 0x1F) * 255) / 31;
    uint32_t a = (c == 0) ? 0 : 255;
    m_StatusClut2[i] = (a << 24) | (b << 16) | (g << 8) | r;
  }
  std::vector<uint32_t> statusClut0(256);
  const uint16_t *pal0Raw =
      reinterpret_cast<const uint16_t *>(timData.data() + 20);
  for (int i = 0; i < 256; ++i) {
    uint16_t c = pal0Raw[i];
    uint32_t r = ((c & 0x1F) * 255) / 31;
    uint32_t g = (((c >> 5) & 0x1F) * 255) / 31;
    uint32_t b = (((c >> 10) & 0x1F) * 255) / 31;
    statusClut0[i] = (255u << 24) | (b << 16) | (g << 8) | r;
  }
  std::ifstream pixFile(itemAllPixPath, std::ios::binary | std::ios::ate);
  if (!pixFile.is_open())
    return false;
  size_t pixSize = static_cast<size_t>(pixFile.tellg());
  pixFile.seekg(0, std::ios::beg);
  m_RawItemAllPix.resize(pixSize);
  pixFile.read(reinterpret_cast<char *>(m_RawItemAllPix.data()), pixSize);
  constexpr uint32_t atlasW = 512;
  constexpr uint32_t atlasH = 512;
  std::vector<uint32_t> atlasPixels(atlasW * atlasH, 0);
  size_t imgHdr = 8 + clutBsize;
  uint16_t iw =
      *reinterpret_cast<const uint16_t *>(timData.data() + imgHdr + 8);
  uint16_t ih =
      *reinterpret_cast<const uint16_t *>(timData.data() + imgHdr + 10);
  uint32_t statusW = iw * 2;
  const uint8_t *statusPix = timData.data() + imgHdr + 12;
  for (uint32_t y = 0; y < ih && y < 256; ++y) {
    for (uint32_t x = 0; x < statusW && x < 256; ++x) {
      atlasPixels[y * atlasW + x] = statusClut0[statusPix[y * statusW + x]];
    }
  }
  for (uint32_t ic = 0; ic < 76 && (ic * 1200 + 1200 <= m_RawItemAllPix.size());
       ++ic) {
    uint32_t col = ic % 6;
    uint32_t row = ic / 6;
    uint32_t dstX = 256 + col * 42;
    uint32_t dstY = row * 32;
    const uint8_t *iconBytes = m_RawItemAllPix.data() + ic * 1200;
    for (uint32_t y = 0; y < 30; ++y) {
      for (uint32_t x = 0; x < 40; ++x) {
        if (dstX + x < atlasW && dstY + y < atlasH) {
          atlasPixels[(dstY + y) * atlasW + (dstX + x)] =
              m_StatusClut2[iconBytes[y * 40 + x]];
        }
      }
    }
  }
  // Render 8x8 font glyphs into atlas at (0..127, 256..303)
  for (int g = 0; g < 95; ++g) {
    int col = g % 16;
    int row = g / 16;
    int gx = col * 8;
    int gy = 256 + row * 8;
    for (int y = 0; y < 8; ++y) {
      uint8_t rowBits = s_Font8x8[g][y];
      for (int x = 0; x < 8; ++x) {
        if ((rowBits >> (7 - x)) & 1) {
          atlasPixels[(gy + y) * atlasW + (gx + x)] = 0xFFFFFFFF;
        }
      }
    }
  }
  // Solid white reference pixel at (511, 511)
  atlasPixels[511 * atlasW + 511] = 0xFFFFFFFF;

  D3D12_HEAP_PROPERTIES defaultHeapProps = {};
  defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = atlasW;
  texDesc.Height = atlasH;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  if (FAILED(m_Device->CreateCommittedResource(
          &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
          IID_PPV_ARGS(&m_StatusTexture))))
    return false;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
  UINT numRows = 0;
  UINT64 rowSizeInBytes = 0;
  UINT64 totalBytes = 0;
  m_Device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, &numRows,
                                  &rowSizeInBytes, &totalBytes);
  Microsoft::WRL::ComPtr<ID3D12Resource> texUpload;
  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC bufDesc = {};
  bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufDesc.Width = totalBytes;
  bufDesc.Height = 1;
  bufDesc.DepthOrArraySize = 1;
  bufDesc.MipLevels = 1;
  bufDesc.SampleDesc.Count = 1;
  bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if (FAILED(m_Device->CreateCommittedResource(
          &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&texUpload))))
    return false;
  uint8_t *pMapped = nullptr;
  texUpload->Map(0, nullptr, reinterpret_cast<void **>(&pMapped));
  for (UINT r = 0; r < numRows; ++r) {
    std::memcpy(pMapped + layout.Offset + r * layout.Footprint.RowPitch,
                atlasPixels.data() + r * atlasW, atlasW * 4);
  }
  texUpload->Unmap(0, nullptr);
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
  m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   IID_PPV_ARGS(&cmdAlloc));
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(),
                              nullptr, IID_PPV_ARGS(&cmdList));
  D3D12_TEXTURE_COPY_LOCATION dst = {};
  dst.pResource = m_StatusTexture.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = 0;
  D3D12_TEXTURE_COPY_LOCATION src = {};
  src.pResource = texUpload.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = layout;
  cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_StatusTexture.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmdList->ResourceBarrier(1, &barrier);
  cmdList->Close();
  ID3D12CommandList *ppCmd[] = {cmdList.Get()};
  m_CmdQueue->ExecuteCommandLists(1, ppCmd);
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  m_CmdQueue->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, hEvent);
  WaitForSingleObjectEx(hEvent, INFINITE, FALSE);
  CloseHandle(hEvent);

  D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
  srvDesc.NumDescriptors = 2;
  srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  m_Device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_SrvHeap));
  D3D12_SHADER_RESOURCE_VIEW_DESC srvView = {};
  srvView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvView.Texture2D.MipLevels = 1;
  m_Device->CreateShaderResourceView(
      m_StatusTexture.Get(), &srvView,
      m_SrvHeap->GetCPUDescriptorHandleForHeapStart());
  m_SrvGpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();

  constexpr UINT maxVerts = 4096;
  D3D12_RESOURCE_DESC vtxDesc = {};
  vtxDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  vtxDesc.Width = maxVerts * sizeof(Render::PS1Vertex);
  vtxDesc.Height = 1;
  vtxDesc.DepthOrArraySize = 1;
  vtxDesc.MipLevels = 1;
  vtxDesc.SampleDesc.Count = 1;
  vtxDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  m_Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
                                    &vtxDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                    nullptr, IID_PPV_ARGS(&m_VertexBuffer));
  D3D12_RANGE readRange = {0, 0};
  m_VertexBuffer->Map(0, &readRange,
                      reinterpret_cast<void **>(&m_VertexBufferMapped));
  m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
  m_VertexBufferView.StrideInBytes = sizeof(Render::PS1Vertex);
  m_VertexBufferView.SizeInBytes = static_cast<UINT>(vtxDesc.Width);

  m_Slots.resize(m_MaxSlots);
  m_Slots[0] = {1, 1};   // Knife
  m_Slots[1] = {2, 15};  // Beretta
  m_Slots[2] = {48, 1};  // Lighter
  m_Slots[3] = {11, 30}; // Handgun Clip
  m_Slots[4] = {65, 1};  // First-Aid Spray
  m_Slots[5] = {68, 1};  // Green Herb
  m_EquippedSlot = 1;
  return true;
}

void InventorySystem::Shutdown() {
  if (m_VertexBuffer && m_VertexBufferMapped) {
    m_VertexBuffer->Unmap(0, nullptr);
    m_VertexBufferMapped = nullptr;
  }
}

void InventorySystem::ToggleOpen() {
  m_IsOpen = !m_IsOpen;
  m_SubmenuActive = false;
  m_Examining = false;
  m_CombineSrcSlot = 0xFF;
  if (m_IsOpen) {
    g_AudioEngine.PlayDoorOpen();
  }
}

bool InventorySystem::AddItem(uint8_t itemId, uint8_t count) {
  if (itemId == 11 || itemId == 12 || itemId == 14 || itemId == 47) {
    for (auto &slot : m_Slots) {
      if (slot.itemId == itemId) {
        slot.count = static_cast<uint8_t>(std::min(250, slot.count + count));
        return true;
      }
    }
  }
  for (auto &slot : m_Slots) {
    if (slot.itemId == 0) {
      slot.itemId = itemId;
      slot.count = count;
      return true;
    }
  }
  return false;
}

bool InventorySystem::RemoveItemByItemId(uint8_t itemId, uint8_t count) {
  for (size_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].itemId == itemId) {
      return RemoveItem(static_cast<uint8_t>(i), count);
    }
  }
  return false;
}

bool InventorySystem::RemoveItem(uint8_t slotIdx, uint8_t count) {
  if (slotIdx >= m_Slots.size() || m_Slots[slotIdx].itemId == 0)
    return false;
  if (m_Slots[slotIdx].count <= count) {
    m_Slots[slotIdx].itemId = 0;
    m_Slots[slotIdx].count = 0;
  } else {
    m_Slots[slotIdx].count -= count;
  }
  return true;
}

bool InventorySystem::CombineItems(uint8_t srcSlot, uint8_t dstSlot) {
  if (srcSlot >= m_Slots.size() || dstSlot >= m_Slots.size())
    return false;
  uint8_t id1 = m_Slots[srcSlot].itemId;
  uint8_t id2 = m_Slots[dstSlot].itemId;
  if (id1 == 0 || id2 == 0)
    return false;
  if ((id1 == 2 && id2 == 11) || (id1 == 11 && id2 == 2)) {
    uint8_t clipSlot = (id1 == 11) ? srcSlot : dstSlot;
    uint8_t gunSlot = (id1 == 2) ? srcSlot : dstSlot;
    m_Slots[gunSlot].count = 15;
    if (m_Slots[clipSlot].count > 15)
      m_Slots[clipSlot].count -= 15;
    else
      m_Slots[clipSlot].itemId = 0;
    g_AudioEngine.PlayWeaponReload();
    return true;
  }
  if ((id1 == 68 && id2 == 67) || (id1 == 67 && id2 == 68)) {
    m_Slots[dstSlot] = {71, 1};
    m_Slots[srcSlot] = {0, 0};
    g_AudioEngine.PlayDoorOpen();
    return true;
  }
  if (id1 == 68 && id2 == 68) {
    m_Slots[dstSlot] = {70, 1};
    m_Slots[srcSlot] = {0, 0};
    g_AudioEngine.PlayDoorOpen();
    return true;
  }
  return false;
}

void InventorySystem::ExecuteAction(InventoryAction action) {
  uint8_t itemId = m_Slots[m_CursorSlot].itemId;
  if (itemId == 0)
    return;
  switch (action) {
  case InventoryAction::Equip:
    m_EquippedSlot = m_CursorSlot;
    g_AudioEngine.PlayDoorOpen();
    m_SubmenuActive = false;
    break;
  case InventoryAction::Use:
    if (itemId == 65 || itemId == 68 || itemId == 70 || itemId == 71) {
      RemoveItem(m_CursorSlot, 1);
      g_AudioEngine.PlayDoorOpen();
      m_SubmenuActive = false;
    } else if (itemId == 1 || itemId == 2 || itemId == 3 || itemId == 5) {
      m_EquippedSlot = m_CursorSlot;
      g_AudioEngine.PlayDoorOpen();
      m_SubmenuActive = false;
    } else {
      // General item usage in world (puzzle items, keys, crests, emblems, etc.)
      m_PendingUsedItemId = itemId;
      m_PendingUsedSlotIdx = m_CursorSlot;
      m_SubmenuActive = false;
      m_IsOpen = false; // Close inventory to apply item in the game world
      g_AudioEngine.PlayMenuConfirm();
    }
    break;
  case InventoryAction::Check:
    m_Examining = true;
    m_ExamineAngle = 0.0f;
    g_AudioEngine.PlayDoorOpen();
    m_SubmenuActive = false;
    break;
  case InventoryAction::Combine:
    m_CombineSrcSlot = m_CursorSlot;
    m_SubmenuActive = false;
    break;
  default:
    break;
  }
}

void InventorySystem::Update(float deltaTime, uint32_t padButtons) {
  if (m_PickupPopupTimer > 0.0f) {
    m_PickupPopupTimer -= deltaTime;
  }
  if (!m_IsOpen)
    return;
  static bool s_PrevUp = false, s_PrevDown = false, s_PrevLeft = false,
              s_PrevRight = false, s_PrevConfirm = false, s_PrevCancel = false;
  bool upRaw = (GetAsyncKeyState(VK_UP) & 0x8000) != 0 ||
               (GetAsyncKeyState('W') & 0x8000) != 0 ||
               (padButtons & (1 << 4)) != 0;
  bool downRaw = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 ||
                 (GetAsyncKeyState('S') & 0x8000) != 0 ||
                 (padButtons & (1 << 5)) != 0;
  bool leftRaw = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0 ||
                 (GetAsyncKeyState('A') & 0x8000) != 0 ||
                 (padButtons & (1 << 6)) != 0;
  bool rightRaw = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0 ||
                  (GetAsyncKeyState('D') & 0x8000) != 0 ||
                  (padButtons & (1 << 7)) != 0;
  bool confirmRaw = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
                    (GetAsyncKeyState('Z') & 0x8000) != 0 ||
                    (GetAsyncKeyState('E') & 0x8000) != 0 ||
                    (padButtons & (1 << 2)) != 0;
  bool cancelRaw = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0 ||
                   (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 ||
                   (GetAsyncKeyState('X') & 0x8000) != 0 ||
                   (padButtons & (1 << 1)) != 0;
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

  if (m_Examining) {
    if (cancel || confirm) {
      m_Examining = false;
      g_AudioEngine.PlayDoorOpen();
      return;
    }
    if (leftRaw)
      m_ExamineAngle -= 2.0f * deltaTime;
    if (rightRaw)
      m_ExamineAngle += 2.0f * deltaTime;
    return;
  }

  if (cancel) {
    if (m_SubmenuActive)
      m_SubmenuActive = false;
    else if (m_CombineSrcSlot != 0xFF)
      m_CombineSrcSlot = 0xFF;
    else {
      ToggleOpen();
      return;
    }
  }

  if (m_SubmenuActive) {
    if (up && m_SubmenuCursor > 0)
      m_SubmenuCursor--;
    if (down && m_SubmenuCursor < 2)
      m_SubmenuCursor++;
    if (confirm) {
      if (m_SubmenuCursor == 0) {
        uint8_t id = m_Slots[m_CursorSlot].itemId;
        if (id == 1 || id == 2 || id == 3 || id == 5)
          ExecuteAction(InventoryAction::Equip);
        else
          ExecuteAction(InventoryAction::Use);
      } else if (m_SubmenuCursor == 1) {
        ExecuteAction(InventoryAction::Check);
      } else if (m_SubmenuCursor == 2) {
        ExecuteAction(InventoryAction::Combine);
      }
    }
  } else {
    if (left && (m_CursorSlot % 2 == 1))
      m_CursorSlot--;
    if (right && (m_CursorSlot % 2 == 0) && (m_CursorSlot + 1 < m_MaxSlots))
      m_CursorSlot++;
    if (up && (m_CursorSlot >= 2))
      m_CursorSlot -= 2;
    if (down && (m_CursorSlot + 2 < m_MaxSlots))
      m_CursorSlot += 2;
    if (confirm) {
      if (m_CombineSrcSlot != 0xFF) {
        if (m_CombineSrcSlot != m_CursorSlot)
          CombineItems(m_CombineSrcSlot, m_CursorSlot);
        m_CombineSrcSlot = 0xFF;
      } else if (m_Slots[m_CursorSlot].itemId != 0) {
        m_SubmenuActive = true;
        m_SubmenuCursor = 0;
      }
    }
  }
}

void InventorySystem::RebuildMenuVertices() {
  if (!m_VertexBufferMapped)
    return;
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
    verts.push_back(q0);
    verts.push_back(q1);
    verts.push_back(q2);
    verts.push_back(q1);
    verts.push_back(q3);
    verts.push_back(q2);
  };

  auto AddSolidQuad = [&](float x, float y, float w, float h, XMFLOAT4 col) {
    AddQuad(x, y, w, h, 511.0f, 511.0f, 511.5f, 511.5f, col);
  };

  auto DrawText = [&](float tx, float ty, const char *str,
                      XMFLOAT4 col = {1, 1, 1, 1}) {
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

  auto DrawWrappedText = [&](float tx, float ty, const char *str,
                             int maxCharsPerLine, XMFLOAT4 col = {1, 1, 1, 1}) {
    float cx = tx;
    float cy = ty;
    int lineCharCount = 0;
    const char *p = str;
    while (*p) {
      const char *wordStart = p;
      while (*p && *p != ' ' && *p != '\n')
        p++;
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

  if (m_Examining) {
    // Authentic Full-Screen PS1 Royal Blue Item Examination Screen
    AddSolidQuad(0, 0, 320, 240, {0.05f, 0.10f, 0.32f, 1.0f});

    // Decorative corner brackets
    AddSolidQuad(12, 12, 20, 2, {0.3f, 0.6f, 0.9f, 0.9f});
    AddSolidQuad(12, 12, 2, 20, {0.3f, 0.6f, 0.9f, 0.9f});
    AddSolidQuad(288, 12, 20, 2, {0.3f, 0.6f, 0.9f, 0.9f});
    AddSolidQuad(306, 12, 2, 20, {0.3f, 0.6f, 0.9f, 0.9f});
    AddSolidQuad(12, 226, 20, 2, {0.3f, 0.6f, 0.9f, 0.9f});
    AddSolidQuad(12, 208, 2, 20, {0.3f, 0.6f, 0.9f, 0.9f});
    AddSolidQuad(288, 226, 20, 2, {0.3f, 0.6f, 0.9f, 0.9f});
    AddSolidQuad(306, 208, 2, 20, {0.3f, 0.6f, 0.9f, 0.9f});

    // Title banner
    AddSolidQuad(60, 14, 200, 18, {0.03f, 0.07f, 0.22f, 0.95f});
    AddSolidQuad(60, 14, 200, 1, {0.35f, 0.55f, 0.85f, 1.0f});
    AddSolidQuad(60, 31, 200, 1, {0.35f, 0.55f, 0.85f, 1.0f});
    DrawText(96, 19, "ITEM EXAMINATION", {1.0f, 0.92f, 0.25f, 1.0f});

    // Center Item Preview
    uint8_t itemId = m_Slots[m_CursorSlot].itemId;
    if (itemId > 0 && itemId < 77) {
      uint8_t iconIdx = itemId - 1;
      uint32_t col = iconIdx % 6;
      uint32_t row = iconIdx / 6;
      float u0 = 256.0f + col * 42.0f;
      float v0 = row * 32.0f;
      float rotOffset = std::sin(m_ExamineAngle) * 8.0f;
      AddQuad(120 + rotOffset, 54, 80, 60, u0, v0, u0 + 40.0f, v0 + 30.0f);
    }
    DrawText(112, 124, "[<] [>] ROTATE", {0.4f, 0.75f, 1.0f, 0.9f});

    // Bottom Detailed Info Panel (Deep Royal Blue)
    AddSolidQuad(16, 146, 288, 76, {0.035f, 0.07f, 0.24f, 0.98f});
    AddSolidQuad(16, 146, 288, 1, {0.4f, 0.6f, 0.9f, 1.0f});
    AddSolidQuad(16, 221, 288, 1, {0.4f, 0.6f, 0.9f, 1.0f});
    AddSolidQuad(16, 146, 1, 76, {0.4f, 0.6f, 0.9f, 1.0f});
    AddSolidQuad(303, 146, 1, 76, {0.4f, 0.6f, 0.9f, 1.0f});

    DrawText(24, 152, GetItemName(itemId), {1.0f, 0.95f, 0.3f, 1.0f});
    DrawWrappedText(24, 166, GetItemDescription(itemId), 33,
                    {0.9f, 0.92f, 0.95f, 1.0f});
    DrawText(88, 208, "[ESC] / [X] : RETURN", {0.45f, 0.75f, 1.0f, 0.9f});

  } else if (m_IsOpen) {
    // Standard Inventory Background
    AddSolidQuad(0, 0, 320, 240, {0.05f, 0.05f, 0.08f, 0.88f});

    // =========================================================================
    // AUTHENTIC PS1 DEEP ROYAL BLUE INFO SCREEN WINDOW (Left Panel)
    // =========================================================================
    float infoX = 14.0f;
    float infoY = 14.0f;
    float infoW = 146.0f;
    float infoH = 148.0f;

    // Beveled frame
    AddSolidQuad(infoX - 2, infoY - 2, infoW + 4, infoH + 4,
                 {0.25f, 0.35f, 0.65f, 1.0f});
    AddSolidQuad(infoX - 1, infoY - 1, infoW + 2, infoH + 2,
                 {0.12f, 0.18f, 0.40f, 1.0f});

    // Deep Royal Blue Background Fill
    AddSolidQuad(infoX, infoY, infoW, infoH, {0.055f, 0.11f, 0.35f, 0.96f});

    // Info Header Banner
    AddSolidQuad(infoX, infoY, infoW, 14, {0.09f, 0.18f, 0.50f, 1.0f});
    AddSolidQuad(infoX, infoY + 14, infoW, 1, {0.35f, 0.55f, 0.85f, 0.8f});
    DrawText(infoX + 38, infoY + 3, "ITEM INFO", {0.8f, 0.92f, 1.0f, 1.0f});

    uint8_t selectedItem = m_Slots[m_CursorSlot].itemId;
    uint8_t selectedCount = m_Slots[m_CursorSlot].count;

    if (selectedItem > 0 && selectedItem < 77) {
      // Large Preview Icon
      uint8_t iconIdx = selectedItem - 1;
      uint32_t col = iconIdx % 6;
      uint32_t row = iconIdx / 6;
      float u0 = 256.0f + col * 42.0f;
      float v0 = row * 32.0f;
      AddQuad(infoX + 43, infoY + 22, 60, 45, u0, v0, u0 + 40.0f, v0 + 30.0f);

      // Item Name
      DrawText(infoX + 8, infoY + 74, GetItemName(selectedItem),
               {1.0f, 0.95f, 0.3f, 1.0f});

      // Ammo / Quantity count
      if (selectedCount > 1 || selectedItem == 2 || selectedItem == 3 ||
          selectedItem == 5) {
        char countStr[32];
        std::sprintf(countStr, "ROUNDS: %d", selectedCount);
        DrawText(infoX + 8, infoY + 86, countStr, {0.85f, 0.9f, 1.0f, 1.0f});
        DrawWrappedText(infoX + 8, infoY + 98, GetItemDescription(selectedItem),
                        16, {0.85f, 0.88f, 0.92f, 1.0f});
      } else {
        DrawWrappedText(infoX + 8, infoY + 88, GetItemDescription(selectedItem),
                        16, {0.85f, 0.88f, 0.92f, 1.0f});
      }
    } else {
      DrawText(infoX + 16, infoY + 68, "NO ITEM SELECTED",
               {0.5f, 0.55f, 0.65f, 1.0f});
    }

    // Bottom-Left EKG Monitor from STATUS.TIM
    AddQuad(16, 170, 140, 56, 12, 170, 152, 226);

    // Right Side 6 Inventory Slots
    constexpr float slotX[2] = {176.0f, 248.0f};
    constexpr float slotY[3] = {24.0f, 76.0f, 128.0f};
    for (uint8_t i = 0; i < m_MaxSlots; ++i) {
      uint8_t c = i % 2;
      uint8_t r = i / 2;
      float sx = slotX[c];
      float sy = slotY[r];
      XMFLOAT4 slotCol =
          (i == m_CursorSlot)
              ? XMFLOAT4(1.0f, 0.95f, 0.2f, 1.0f)
              : ((i == m_EquippedSlot) ? XMFLOAT4(0.2f, 0.9f, 0.3f, 1.0f)
                                       : XMFLOAT4(0.4f, 0.45f, 0.55f, 0.8f));
      AddSolidQuad(sx - 2, sy - 2, 44, 34, slotCol);
      AddSolidQuad(sx, sy, 40, 30, {0.06f, 0.08f, 0.15f, 0.95f});

      uint8_t itemId = m_Slots[i].itemId;
      if (itemId > 0 && itemId < 77) {
        uint8_t iconIdx = itemId - 1;
        uint32_t col = iconIdx % 6;
        uint32_t row = iconIdx / 6;
        float u0 = 256.0f + col * 42.0f;
        float v0 = row * 32.0f;
        AddQuad(sx, sy, 40, 30, u0, v0, u0 + 40.0f, v0 + 30.0f);
        if (m_Slots[i].count > 1) {
          char numStr[16];
          std::sprintf(numStr, "%d", m_Slots[i].count);
          DrawText(sx + 22, sy + 20, numStr, {1.0f, 1.0f, 1.0f, 1.0f});
        }
      }
    }

    // Submenu
    if (m_SubmenuActive) {
      float smX = 168.0f;
      float smY = 56.0f;
      AddSolidQuad(smX - 2, smY - 2, 76, 58, {0.3f, 0.45f, 0.75f, 1.0f});
      AddSolidQuad(smX, smY, 72, 54, {0.04f, 0.08f, 0.25f, 0.98f});

      uint8_t cId = m_Slots[m_CursorSlot].itemId;
      bool isWep = (cId == 1 || cId == 2 || cId == 3 || cId == 5);
      const char *opts[3] = {isWep ? "EQUIP" : "USE", "CHECK", "COMBINE"};
      for (uint8_t opt = 0; opt < 3; ++opt) {
        XMFLOAT4 optCol = (opt == m_SubmenuCursor)
                              ? XMFLOAT4(1.0f, 0.92f, 0.25f, 1.0f)
                              : XMFLOAT4(0.7f, 0.75f, 0.85f, 1.0f);
        if (opt == m_SubmenuCursor) {
          DrawText(smX + 4, smY + 6 + opt * 16, ">", optCol);
        }
        DrawText(smX + 16, smY + 6 + opt * 16, opts[opt], optCol);
      }
    }
  }

  // World Pickup Notification Popup Banner
  if (m_PickupPopupTimer > 0.0f) {
    float px = 40.0f;
    float py = 192.0f;
    float pw = 240.0f;
    float ph = 38.0f;
    AddSolidQuad(px - 2, py - 2, pw + 4, ph + 4, {0.35f, 0.55f, 0.85f, 1.0f});
    AddSolidQuad(px, py, pw, ph, {0.05f, 0.12f, 0.38f, 0.95f});

    if (m_PickupPopupItemId > 0 && m_PickupPopupItemId < 77) {
      uint8_t iconIdx = m_PickupPopupItemId - 1;
      uint32_t col = iconIdx % 6;
      uint32_t row = iconIdx / 6;
      float u0 = 256.0f + col * 42.0f;
      float v0 = row * 32.0f;
      AddQuad(px + 6, py + 4, 40, 30, u0, v0, u0 + 40.0f, v0 + 30.0f);
    }
    DrawText(px + 54, py + 8, "FOUND ITEM:", {0.5f, 0.85f, 1.0f, 1.0f});
    DrawText(px + 54, py + 20, GetItemName(m_PickupPopupItemId),
             {1.0f, 0.95f, 0.3f, 1.0f});
  }

  m_VertexCount = static_cast<uint32_t>(verts.size());
  std::memcpy(m_VertexBufferMapped, verts.data(),
              verts.size() * sizeof(Render::PS1Vertex));
}

void InventorySystem::Render(ID3D12GraphicsCommandList *cmdList,
                             Render::PS1Pipeline &pipeline) {
  if ((!m_IsOpen && m_PickupPopupTimer <= 0.0f) || !m_VertexBufferMapped)
    return;
  RebuildMenuVertices();
  if (m_VertexCount == 0)
    return;

  pipeline.Bind(cmdList);
  Render::SceneConstants constants;
  constants.worldViewProj = XMMatrixIdentity();
  constants.prevWorldViewProj = XMMatrixIdentity();
  constants.screenResolution =
      XMFLOAT4(320.0f, 240.0f, 1.0f / 320.0f, 1.0f / 240.0f);
  constants.renderFlags = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
  constants.blendMode = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  constants.jitterOffset = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
  pipeline.UpdateConstants(cmdList, constants);

  ID3D12DescriptorHeap *ppHeaps[] = {m_SrvHeap.Get()};
  cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  cmdList->SetGraphicsRootDescriptorTable(1, m_SrvGpuHandle);
  cmdList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(m_VertexCount, 1, 0, 0);
}

} // namespace Game
