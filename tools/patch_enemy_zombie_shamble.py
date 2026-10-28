with open('port/src/game/enemy_zombie.cpp', 'r', encoding='utf-8') as f:
    cpp = f.read()

old_shamble = """    // Shamble forward along ground plane
    float moveSpeed = 42.0f; // Classic sluggish zombie pace
    m_Position.x += std::sin(m_FacingAngle) * (moveSpeed * deltaTime);
    m_Position.z += std::cos(m_FacingAngle) * (moveSpeed * deltaTime);

    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, m_CollisionRadius);
    }"""

new_shamble = """    // Shamble forward along ground plane
    float moveSpeed = 42.0f; // Classic sluggish zombie pace
    float oldX = m_Position.x;
    float oldZ = m_Position.z;
    m_Position.x += std::sin(m_FacingAngle) * (moveSpeed * deltaTime);
    m_Position.z += std::cos(m_FacingAngle) * (moveSpeed * deltaTime);

    if (roomMgr) {
      roomMgr->ResolveCollision(m_Position.x, m_Position.z, oldX, oldZ, m_CollisionRadius);
    }"""

if old_shamble in cpp:
    cpp = cpp.replace(old_shamble, new_shamble)
    with open('port/src/game/enemy_zombie.cpp', 'w', encoding='utf-8') as f:
        f.write(cpp)
    print('Shamble collision patched successfully.')
else:
    print('Error: old_shamble not found.')
