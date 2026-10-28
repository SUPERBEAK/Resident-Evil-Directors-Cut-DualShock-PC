with open('port/src/room_manager.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

impl = '''
bool RoomManager::LoadRoomAtPosition(uint16_t stageId, uint8_t roomId, float playerX, float playerZ) {
  if (!LoadRoom(stageId, roomId, 0)) {
    return false;
  }

  uint32_t bestCut = FindCameraForPosition(playerX, playerZ);
  m_activeCut = bestCut;
  std::wprintf(L"[RoomManager] Position (%.1f, %.1f) selected initial Camera Cut %u\\n",
               playerX, playerZ, m_activeCut);
  return true;
}

uint32_t RoomManager::FindCameraForPosition(float playerX, float playerZ) const {
  if (!m_currentRoom || m_currentRoom->cameras.empty()) {
    return 0;
  }

  uint32_t bestCam = 0;
  float bestScore = -1e9f;

  for (uint32_t i = 0; i < m_currentRoom->cameras.size(); ++i) {
    const auto& cam = m_currentRoom->cameras[i];
    float fx = cam.to.x - cam.from.x;
    float fz = cam.to.z - cam.from.z;
    float flen = std::sqrt(fx * fx + fz * fz);
    if (flen < 1.0f) continue;
    fx /= flen;
    fz /= flen;

    float dx = playerX - cam.from.x;
    float dz = playerZ - cam.from.z;
    float forwardDist = dx * fx + dz * fz;

    // Player must be in front of the camera (not behind)
    if (forwardDist <= 50.0f) continue;

    float totDist = std::sqrt(dx * dx + dz * dz);
    float cosAngle = (totDist > 0.0f) ? (forwardDist / totDist) : 0.0f;
    float tgtDist = std::sqrt((playerX - cam.to.x) * (playerX - cam.to.x) +
                              (playerZ - cam.to.z) * (playerZ - cam.to.z));

    // Check if player is within this camera's bounding switch zone
    bool inQuad = false;
    for (const auto& zone : m_currentRoom->switchZones) {
      if (zone.camFrom == i && zone.camTo >= m_currentRoom->numCameras) {
        if (IsPointInQuad(playerX, playerZ, zone)) {
          inQuad = true;
          break;
        }
      }
    }

    // High cosine alignment with camera forward vector + proximity to look target + quad containment
    float score = (cosAngle * 1000.0f) - (tgtDist * 0.05f) + (inQuad ? 2000.0f : 0.0f);
    if (score > bestScore) {
      bestScore = score;
      bestCam = i;
    }
  }

  return bestCam;
}
'''

pos = text.find('bool RoomManager::LoadRoom(')
if pos != -1:
    end_load = text.find('return true;', pos)
    end_brace = text.find('}', end_load) + 1
    text = text[:end_brace] + '\n' + impl + text[end_brace:]
    with open('port/src/room_manager.cpp', 'w', encoding='utf-8') as f:
        f.write(text)
    print('SUCCESS: Added LoadRoomAtPosition and FindCameraForPosition')
else:
    print('ERROR: LoadRoom not found')
