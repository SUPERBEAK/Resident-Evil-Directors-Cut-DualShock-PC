#pragma once
#include "ps1_pipeline.h"
#include <cstdint>
#include <string>
#include <vector>

namespace PSX {

struct TmdMesh {
  std::vector<Render::PS1Vertex> vertices;
};

class TmdLoader {
public:
  static bool LoadFromFile(const std::wstring &filePath,
                           std::vector<TmdMesh> &outMeshes,
                           float scale = 1.0f);
  static bool LoadFromMemory(const uint8_t *data, size_t size,
                             std::vector<TmdMesh> &outMeshes,
                             float scale = 1.0f);
};

} // namespace PSX
