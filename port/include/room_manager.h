#pragma once



#include <DirectXMath.h>

#include <cstdint>

#include <memory>

#include <string>

#include <vector>



/**

 * Represents an authentic Resident Evil 1 door trigger record.

 * Parsed directly from RDT initialization script (SCD opcode 0x0C: door_set).

 */

struct RoomDoor {

  uint8_t slot = 0;

  uint16_t x = 0;

  uint16_t z = 0;

  uint16_t w = 0;

  uint16_t d = 0;          // Bounding rectangle in PS1 XZ coordinates

  uint8_t lock = 0;        // Lock flag bitfield

  uint8_t targetStage = 0; // Destination stage (0 = same stage)

  uint8_t targetRoom = 0;  // Destination room ID

  uint16_t nextX = 0;

  int16_t nextY = 0;

  uint16_t nextZ = 0;  // Destination player position

  int16_t nextDir = 0; // Destination player facing angle (0..4095)

  uint8_t keyItem = 0; // Key required to unlock

  bool isLocked = false;

};



/**

 * Represents a fixed camera cut perspective in a room.

 * Parsed from RDT camera records (+0x94, RdtCamera 0x2C bytes each).

 */

struct RoomCamera {

  DirectX::XMFLOAT3 from = {0.0f, 0.0f, 0.0f}; // Camera eye position

  DirectX::XMFLOAT3 to = {0.0f, 0.0f, 0.0f};   // Camera look target

  float fov = 220.0f; // PS1 projection distance (approx 220-240)

  float roll = 0.0f;  // Camera roll

};



/**

 * Represents a camera switch trigger zone (convex quad).

 * Parsed from RDT camera switch table (+0x48, CamSwitchZone 0x14 bytes each).

 */

struct RoomCamSwitch {

  uint16_t camTo = 0;

  uint16_t camFrom = 0;

  uint16_t x0 = 0, z0 = 0;

  uint16_t x1 = 0, z1 = 0;

  uint16_t x2 = 0, z2 = 0;

  uint16_t x3 = 0, z3 = 0;

};



/**

 * Active room state and parsed RDT assets.

 */

/**

 * Authentic PS1 Resident Evil 1 SCA Room Collision Boundary.

 * Parsed from RDT header offset +0x4C (ScaBoundary 12 bytes each).

 */

struct RoomCollisionBox {

  uint16_t x1 = 0; // Max X (PS1 coordinates 0..65535)

  uint16_t z1 = 0; // Max Z (PS1 coordinates 0..65535)

  uint16_t x2 = 0; // Min X (PS1 coordinates 0..65535)

  uint16_t z2 = 0; // Min Z (PS1 coordinates 0..65535)

  uint16_t type = 0; // 1 = AABB rect, 3 = circle, 4 = stairs, 5 = enemy barrier

  uint16_t floor = 0; // Floor height / 256 and flags

};



struct RoomFloorZone {
  int16_t x = 0;
  int16_t z = 0;
  uint16_t w = 0;
  uint16_t d = 0;
  uint8_t seBase = 45;
  uint8_t pad = 0;
};

struct RoomData {

  uint16_t stageId = 0;

  uint8_t roomId = 0;

  std::wstring bssPath;

  std::wstring rdtPath;

  uint8_t numCameras = 0;

  uint8_t ambientR = 0;

  uint8_t ambientG = 0;

  uint8_t ambientB = 0;

  std::vector<RoomCamera> cameras;

  std::vector<RoomCamSwitch> switchZones;

  std::vector<RoomDoor> doors;

  std::vector<uint8_t> rdtRaw;

  int16_t scaSplitX = 0;

  int16_t scaSplitZ = 0;

  uint32_t scaCounts[4] = {0, 0, 0, 0};

  std::vector<RoomCollisionBox> collisionBoxes;
  std::vector<RoomFloorZone> floorZones;

};



/**

 * Room Manager: authentic stage and room progression, dynamic camera switching,

 * and door transition trigger handler for Resident Evil Director's Cut.

 */

class RoomManager {

public:

  RoomManager();

  ~RoomManager();



  bool Initialize(const std::wstring &baseExtractionDir);

  bool LoadRoom(uint16_t stageId, uint8_t roomId, uint32_t initialCut = 0);

  bool LoadRoomAtPosition(uint16_t stageId, uint8_t roomId, float playerX, float playerZ);

  uint32_t FindCameraForPosition(float playerX, float playerZ) const;



  // Updates camera zones based on player position. Returns true if camera cut

  // changed.

  bool UpdateCameraZone(float playerX, float playerZ, uint32_t &outNewCut);



  // Checks if player is within any door trigger zone.

  bool CheckDoorTrigger(float playerX, float playerZ, float facingAngle, RoomDoor &outDoor) const;



  // Resolves player/entity collision against room boundaries using authentic PS1 physics

  bool ResolveCollision(float &posX, float &posZ, float oldX, float oldZ, float radius = 422.0f, float posY = 0.0f) const;

  // Computes a collision-free 2D A* waypoint path from (startX, startZ) to a target door
  void FindPathToDoor(float startX, float startZ, float posY, const RoomDoor &door, std::vector<DirectX::XMFLOAT2> &outWaypoints) const;

  // Tracks player position/orientation at door entry and crosschecks 3D door vs active 320x240 camera screen
  bool CrosscheckDoorEntry(float playerX, float playerY, float playerZ, float facingAngle, const RoomDoor &door) const;

  uint8_t LookupFootstepZone(float playerX, float playerZ) const;



  // Manual camera cut selection

  bool SetActiveCut(uint32_t cutIndex);



  // Accessors

  uint32_t GetActiveCut() const { return m_activeCut; }

  const RoomCamera *GetActiveCamera() const;

  const RoomCamera *GetCamera(uint32_t index) const;

  uint8_t GetNumCameras() const {

    return m_currentRoom ? m_currentRoom->numCameras : 0;

  }

  const std::wstring &GetCurrentBssPath() const {

    return m_currentRoom ? m_currentRoom->bssPath : m_emptyWString;

  }

  const std::wstring &GetCurrentRdtPath() const {

    return m_currentRoom ? m_currentRoom->rdtPath : m_emptyWString;

  }

  const std::vector<uint8_t> &GetCurrentRdtData() const {

    return m_currentRoom ? m_currentRoom->rdtRaw : m_emptyByteVector;

  }

  uint16_t GetCurrentStage() const {

    return m_currentRoom ? m_currentRoom->stageId : 0;

  }

  uint8_t GetCurrentRoomId() const {

    return m_currentRoom ? m_currentRoom->roomId : 0;

  }

  const std::vector<RoomDoor> &GetDoors() const {

    return m_currentRoom ? m_currentRoom->doors : m_emptyDoorVector;

  }

  bool HasActiveRoom() const { return m_currentRoom != nullptr; }



private:

  std::wstring m_baseDir;

  std::unique_ptr<RoomData> m_currentRoom;

  uint32_t m_activeCut = 0;



  static bool IsPointInQuad(float posX, float posZ, const RoomCamSwitch &z);

  bool ParseRdt(const std::vector<uint8_t> &data, RoomData &room);



  std::wstring m_emptyWString;

  std::vector<uint8_t> m_emptyByteVector;

  std::vector<RoomDoor> m_emptyDoorVector;

};

