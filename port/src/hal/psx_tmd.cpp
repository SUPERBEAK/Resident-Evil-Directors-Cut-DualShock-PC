#include "psx_tmd.h"
#include <cstdio>
#include <cstring>
#include <fstream>

namespace PSX {

#pragma pack(push, 1)
struct TmdHeader {
  uint32_t id;
  uint32_t flags;
  uint32_t numObjects;
};

struct TmdObject {
  uint32_t vertTop;
  uint32_t nVert;
  uint32_t normTop;
  uint32_t nNorm;
  uint32_t primTop;
  uint32_t nPrim;
  int32_t scale;
};

struct TmdVertex {
  int16_t vx, vy, vz, pad;
};
#pragma pack(pop)

bool TmdLoader::LoadFromFile(const std::wstring &filePath,
                             std::vector<TmdMesh> &outMeshes, float scale) {
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  size_t size = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  file.read(reinterpret_cast<char *>(buffer.data()), size);
  return LoadFromMemory(buffer.data(), size, outMeshes, scale);
}

bool TmdLoader::LoadFromMemory(const uint8_t *data, size_t size,
                               std::vector<TmdMesh> &outMeshes, float scale) {
  if (!data || size < 12)
    return false;

  const TmdHeader *hdr = reinterpret_cast<const TmdHeader *>(data);
  if (hdr->id != 0x41 && !(hdr->numObjects > 0 && hdr->numObjects <= 64))
    return false; // PS1 TMD signature or valid Capcom EMD object count

  outMeshes.resize(hdr->numObjects);

  for (uint32_t objIdx = 0; objIdx < hdr->numObjects; ++objIdx) {
    size_t objTableOffset = 12 + objIdx * sizeof(TmdObject);
    if (objTableOffset + sizeof(TmdObject) > size)
      break;

    const TmdObject *obj =
        reinterpret_cast<const TmdObject *>(data + objTableOffset);
    TmdMesh &mesh = outMeshes[objIdx];

    // Read vertices and normals tables
    const uint8_t *basePtr = data + 12;
    const TmdVertex *verts =
        reinterpret_cast<const TmdVertex *>(basePtr + obj->vertTop);
    const TmdVertex *norms =
        reinterpret_cast<const TmdVertex *>(basePtr + obj->normTop);
    const uint8_t *primPtr = basePtr + obj->primTop;

    auto CalcLight = [&](uint16_t normIdx) -> float {
      if (normIdx >= obj->nNorm)
        return 0.85f;
      float nx = norms[normIdx].vx / 4096.0f;
      float ny = -norms[normIdx].vy / 4096.0f;
      float nz = norms[normIdx].vz / 4096.0f;
      float dot = nx * 0.4f + ny * 0.7f + nz * 0.6f;
      float intensity = 0.35f + 0.65f * (dot > 0.0f ? dot : 0.0f);
      return (intensity < 0.25f) ? 0.25f
                                 : ((intensity > 1.0f) ? 1.0f : intensity);
    };

    // Parse primitive packet records
    for (uint32_t p = 0; p < obj->nPrim; ++p) {
      uint8_t olen = primPtr[0];
      uint8_t ilen = primPtr[1];
      uint8_t flag = primPtr[2];
      uint8_t mode = primPtr[3];

      // Mode bits: bit 2 = quad (4 verts), bit 0 = shaded, bit 1 = textured
      bool isQuad = (mode & 0x08) != 0;
      bool isTextured = (mode & 0x04) != 0;

      if (!isTextured) {
        // Monochrome or Gouraud Triangle
        if (!isQuad) {
          uint8_t r = primPtr[4], g = primPtr[5], b = primPtr[6];
          const uint16_t *indices =
              reinterpret_cast<const uint16_t *>(primPtr + 8);
          uint16_t i0 = indices[0], i1 = indices[1], i2 = indices[2];

          if (i0 < obj->nVert && i1 < obj->nVert && i2 < obj->nVert) {
            Render::PS1Vertex v0{{verts[i0].vx * scale, verts[i0].vy * scale,
                                  verts[i0].vz * scale},
                                 {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f},
                                 {0.0f, 0.0f}};
            Render::PS1Vertex v1{{verts[i1].vx * scale, verts[i1].vy * scale,
                                  verts[i1].vz * scale},
                                 {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f},
                                 {1.0f, 0.0f}};
            Render::PS1Vertex v2{{verts[i2].vx * scale, verts[i2].vy * scale,
                                  verts[i2].vz * scale},
                                 {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f},
                                 {0.5f, 1.0f}};
            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v2);
          }
        } else {
          // Monochrome Quad
          uint8_t r = primPtr[4], g = primPtr[5], b = primPtr[6];
          const uint16_t *indices =
              reinterpret_cast<const uint16_t *>(primPtr + 8);
          uint16_t i0 = indices[0], i1 = indices[1], i2 = indices[2],
                   i3 = indices[3];

          if (i0 < obj->nVert && i1 < obj->nVert && i2 < obj->nVert &&
              i3 < obj->nVert) {
            Render::PS1Vertex v0{{verts[i0].vx * scale, verts[i0].vy * scale,
                                  verts[i0].vz * scale},
                                 {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f},
                                 {0.0f, 0.0f}};
            Render::PS1Vertex v1{{verts[i1].vx * scale, verts[i1].vy * scale,
                                  verts[i1].vz * scale},
                                 {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f},
                                 {1.0f, 0.0f}};
            Render::PS1Vertex v2{{verts[i2].vx * scale, verts[i2].vy * scale,
                                  verts[i2].vz * scale},
                                 {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f},
                                 {0.0f, 1.0f}};
            Render::PS1Vertex v3{{verts[i3].vx * scale, verts[i3].vy * scale,
                                  verts[i3].vz * scale},
                                 {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f},
                                 {1.0f, 1.0f}};

            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v2);

            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v3);
            mesh.vertices.push_back(v2);
          }
        }
      } else {
        // Textured Triangle or Quad (TG3 / TG4)
        if (!isQuad) {
          uint8_t u0 = primPtr[4], v0_uv = primPtr[5];
          uint8_t u1 = primPtr[8], v1_uv = primPtr[9];
          uint8_t u2 = primPtr[12], v2_uv = primPtr[13];

          uint16_t tsb = *reinterpret_cast<const uint16_t *>(primPtr + 10);
          float u_off0 = ((tsb & 1) && u0 < 128) ? 128.0f : 0.0f;
          float u_off1 = ((tsb & 1) && u1 < 128) ? 128.0f : 0.0f;
          float u_off2 = ((tsb & 1) && u2 < 128) ? 128.0f : 0.0f;

          const uint16_t *p0 = reinterpret_cast<const uint16_t *>(primPtr + 16);
          const uint16_t *p1 = reinterpret_cast<const uint16_t *>(primPtr + 20);
          const uint16_t *p2 = reinterpret_cast<const uint16_t *>(primPtr + 24);
          uint16_t n0 = p0[0], i0 = p0[1];
          uint16_t n1 = p1[0], i1 = p1[1];
          uint16_t n2 = p2[0], i2 = p2[1];

          if (i0 < obj->nVert && i1 < obj->nVert && i2 < obj->nVert) {
            float l0 = CalcLight(n0), l1 = CalcLight(n1), l2 = CalcLight(n2);
            Render::PS1Vertex v0{{verts[i0].vx * scale, verts[i0].vy * scale,
                                  verts[i0].vz * scale},
                                 {l0, l0, l0, 1.0f},
                                 {(u0 + u_off0) / 255.0f, v0_uv / 255.0f}};
            Render::PS1Vertex v1{{verts[i1].vx * scale, verts[i1].vy * scale,
                                  verts[i1].vz * scale},
                                 {l1, l1, l1, 1.0f},
                                 {(u1 + u_off1) / 255.0f, v1_uv / 255.0f}};
            Render::PS1Vertex v2{{verts[i2].vx * scale, verts[i2].vy * scale,
                                  verts[i2].vz * scale},
                                 {l2, l2, l2, 1.0f},
                                 {(u2 + u_off2) / 255.0f, v2_uv / 255.0f}};
            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v2);
          }
        } else {
          // TG4 (Textured Quad)
          uint8_t u0 = primPtr[4], v0_uv = primPtr[5];
          uint8_t u1 = primPtr[8], v1_uv = primPtr[9];
          uint8_t u2 = primPtr[12], v2_uv = primPtr[13];
          uint8_t u3 = primPtr[16], v3_uv = primPtr[17];

          uint16_t tsb = *reinterpret_cast<const uint16_t *>(primPtr + 10);
          float u_off0 = ((tsb & 1) && u0 < 128) ? 128.0f : 0.0f;
          float u_off1 = ((tsb & 1) && u1 < 128) ? 128.0f : 0.0f;
          float u_off2 = ((tsb & 1) && u2 < 128) ? 128.0f : 0.0f;
          float u_off3 = ((tsb & 1) && u3 < 128) ? 128.0f : 0.0f;

          const uint16_t *p0 = reinterpret_cast<const uint16_t *>(primPtr + 20);
          const uint16_t *p1 = reinterpret_cast<const uint16_t *>(primPtr + 24);
          const uint16_t *p2 = reinterpret_cast<const uint16_t *>(primPtr + 28);
          const uint16_t *p3 = reinterpret_cast<const uint16_t *>(primPtr + 32);
          uint16_t n0 = p0[0], i0 = p0[1];
          uint16_t n1 = p1[0], i1 = p1[1];
          uint16_t n2 = p2[0], i2 = p2[1];
          uint16_t n3 = p3[0], i3 = p3[1];

          if (i0 < obj->nVert && i1 < obj->nVert && i2 < obj->nVert &&
              i3 < obj->nVert) {
            float l0 = CalcLight(n0), l1 = CalcLight(n1), l2 = CalcLight(n2),
                  l3 = CalcLight(n3);
            Render::PS1Vertex v0{{verts[i0].vx * scale, verts[i0].vy * scale,
                                  verts[i0].vz * scale},
                                 {l0, l0, l0, 1.0f},
                                 {(u0 + u_off0) / 255.0f, v0_uv / 255.0f}};
            Render::PS1Vertex v1{{verts[i1].vx * scale, verts[i1].vy * scale,
                                  verts[i1].vz * scale},
                                 {l1, l1, l1, 1.0f},
                                 {(u1 + u_off1) / 255.0f, v1_uv / 255.0f}};
            Render::PS1Vertex v2{{verts[i2].vx * scale, verts[i2].vy * scale,
                                  verts[i2].vz * scale},
                                 {l2, l2, l2, 1.0f},
                                 {(u2 + u_off2) / 255.0f, v2_uv / 255.0f}};
            Render::PS1Vertex v3{{verts[i3].vx * scale, verts[i3].vy * scale,
                                  verts[i3].vz * scale},
                                 {l3, l3, l3, 1.0f},
                                 {(u3 + u_off3) / 255.0f, v3_uv / 255.0f}};
            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v2);

            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v3);
            mesh.vertices.push_back(v2);
          }
        }
      }

      // In PS1 TMD format, packet size in file is (ilen + 1) * 4 bytes
      primPtr += (ilen + 1) * 4;
      if (primPtr >= data + size)
        break;
    }

    std::printf("[TMD] Loaded Object %u: %zu render vertices\n", objIdx,
                mesh.vertices.size());
  }

  return true;
}

} // namespace PSX
