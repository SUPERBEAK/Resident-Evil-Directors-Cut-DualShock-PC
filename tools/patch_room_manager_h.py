import os

file_path = "port/include/room_manager.h"
with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

# Replace RoomDoor int16_t with uint16_t for x, z, w, d, nextX, nextZ
old_door = """struct RoomDoor {
  uint8_t slot = 0;
  int16_t x = 0;
  int16_t z = 0;
  int16_t w = 0;
  int16_t d = 0;           // Bounding rectangle in PS1 XZ coordinates
  uint8_t lock = 0;        // Lock flag bitfield
  uint8_t targetStage = 0; // Destination stage (0 = same stage)
  uint8_t targetRoom = 0;  // Destination room ID
  int16_t nextX = 0;
  int16_t nextY = 0;
  int16_t nextZ = 0;   // Destination player position
  int16_t nextDir = 0; // Destination player facing angle (0..4095)
  uint8_t keyItem = 0; // Key required to unlock
  bool isLocked = false;
};"""

new_door = """struct RoomDoor {
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
};"""

old_switch = """struct RoomCamSwitch {
  uint16_t camTo = 0;
  uint16_t camFrom = 0;
  int16_t x0 = 0, z0 = 0;
  int16_t x1 = 0, z1 = 0;
  int16_t x2 = 0, z2 = 0;
  int16_t x3 = 0, z3 = 0;
};"""

new_switch = """struct RoomCamSwitch {
  uint16_t camTo = 0;
  uint16_t camFrom = 0;
  uint16_t x0 = 0, z0 = 0;
  uint16_t x1 = 0, z1 = 0;
  uint16_t x2 = 0, z2 = 0;
  uint16_t x3 = 0, z3 = 0;
};"""

content = content.replace(old_door.replace("\r\n", "\n"), new_door.replace("\r\n", "\n"))
content = content.replace(old_switch.replace("\r\n", "\n"), new_switch.replace("\r\n", "\n"))

# Ensure CRLF line endings
content = content.replace("\r\n", "\n").replace("\n", "\r\n")

with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)

print("Successfully patched port/include/room_manager.h with uint16_t door and camera switch coordinates.")
