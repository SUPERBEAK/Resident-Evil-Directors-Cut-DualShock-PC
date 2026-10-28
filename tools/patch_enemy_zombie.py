# Patch enemy_zombie.h
with open('port/include/enemy_zombie.h', 'r', encoding='utf-8') as f:
    hdr = f.read()

if '#include "player_character.h"' not in hdr:
    hdr = hdr.replace('#include "psx_tmd.h"\n', '#include "psx_tmd.h"\n#include "player_character.h"\n')
    with open('port/include/enemy_zombie.h', 'w', encoding='utf-8') as f:
        f.write(hdr)
    print('enemy_zombie.h patched with player_character.h include.')

# Patch enemy_zombie.cpp
with open('port/src/game/enemy_zombie.cpp', 'r', encoding='utf-8') as f:
    cpp = f.read()

# 1. pos -> position
old_vertex_loop = """    for (const auto &v : mesh.vertices) {
      XMVECTOR pos = XMVectorSet(v.pos.x, v.pos.y, v.pos.z, 1.0f);
      XMVECTOR transformed = XMVector3Transform(pos, jointMat);

      Render::PS1Vertex outV = v;
      XMStoreFloat3(&outV.pos, transformed);
      m_CurrentPoseVertices[outIdx++] = outV;
    }"""

new_vertex_loop = """    for (const auto &v : mesh.vertices) {
      XMVECTOR pos = XMVectorSet(v.position.x, v.position.y, v.position.z, 1.0f);
      XMVECTOR transformed = XMVector3Transform(pos, jointMat);

      Render::PS1Vertex outV = v;
      XMStoreFloat3(&outV.position, transformed);
      m_CurrentPoseVertices[outIdx++] = outV;
    }"""

if old_vertex_loop in cpp:
    cpp = cpp.replace(old_vertex_loop, new_vertex_loop)
    print('Vertex loop patched.')
else:
    print('Warning: old_vertex_loop not found.')

# 2. HitRecoil ResolveCollision
old_recoil_coll = """    // Push backwards from gunshot impact
    m_Position.x -= std::sin(m_FacingAngle) * (40.0f * deltaTime * 60.0f);
    m_Position.z -= std::cos(m_FacingAngle) * (40.0f * deltaTime * 60.0f);
    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, m_CollisionRadius);
    }"""

new_recoil_coll = """    // Push backwards from gunshot impact
    float oldX = m_Position.x;
    float oldZ = m_Position.z;
    m_Position.x -= std::sin(m_FacingAngle) * (40.0f * deltaTime * 60.0f);
    m_Position.z -= std::cos(m_FacingAngle) * (40.0f * deltaTime * 60.0f);
    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, oldX, oldZ, m_CollisionRadius);
    }"""

if old_recoil_coll in cpp:
    cpp = cpp.replace(old_recoil_coll, new_recoil_coll)
    print('Recoil collision patched.')
else:
    print('Warning: old_recoil_coll not found.')

# 3. Shamble ResolveCollision
old_shamble_coll = """    // Shamble forward along ground plane
    float moveSpeed = 42.0f;
    m_Position.x += std::sin(m_FacingAngle) * (moveSpeed * deltaTime);
    m_Position.z += std::cos(m_FacingAngle) * (moveSpeed * deltaTime);

    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, m_CollisionRadius);
    }"""

new_shamble_coll = """    // Shamble forward along ground plane
    float moveSpeed = 42.0f;
    float oldX = m_Position.x;
    float oldZ = m_Position.z;
    m_Position.x += std::sin(m_FacingAngle) * (moveSpeed * deltaTime);
    m_Position.z += std::cos(m_FacingAngle) * (moveSpeed * deltaTime);

    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, oldX, oldZ, m_CollisionRadius);
    }"""

if old_shamble_coll in cpp:
    cpp = cpp.replace(old_shamble_coll, new_shamble_coll)
    print('Shamble collision patched.')
else:
    print('Warning: old_shamble_coll not found.')

with open('port/src/game/enemy_zombie.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp)
print('enemy_zombie.cpp written.')
