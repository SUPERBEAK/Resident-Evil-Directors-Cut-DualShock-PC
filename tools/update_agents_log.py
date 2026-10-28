with open('AGENTS.md', 'r', encoding='utf-8') as f:
    content = f.read()

new_log = """- **2026-10-25 [Antigravity]**: Resolved Door Interaction Failure & Implemented Authentic PS1 Forward Probe:
  1. Root cause 1: In original PS1 engine (check_action_object in dualshock/src/main/room_action.c), doors and interactive objects are probed using a 600-unit forward vector (*s = 0x258; rotate_vector_y(...)) projected ahead of the player's facing direction. Previously, CheckDoorTrigger only tested the player's center position (playerX, playerZ). Because SCA collision walls halt Chris at Z ~ 2380 before his center reaches the door trigger box [500, 2300], Chris could never trigger the door.
  2. Root cause 2: In original RE1 engine (door_try_enter in dualshock/src/main/room_action.c), bit 7 (0x80) of lock is the lock indicator flag ((flags & 0x80)). Previously, room_manager.cpp checked (door.lock != 0), which mistakenly locked doors that were completely unlocked but had low-bit attributes.
  3. Root cause 3: In main.cpp, Enter and Space were shared between combat firing and room interaction without filtering out aiming states.
  4. Fixed: Implemented authentic 600-unit forward probe in RoomManager::CheckDoorTrigger (probeX = playerX + sin(facingAngle) * 600.0f, probeZ = playerZ + cos(facingAngle) * 600.0f), testing both probe and player position against bounding boxes and door center radius. Corrected lock mask to (door.lock & 0x80) != 0, filtered action keys while aiming, and mapped E/Enter/Z across dual-channel keyboard and gamepad inputs. Verified door transitions between Mansion Dining Room (Room 100) and Main Hall (Room 101) with authentic SPU door creak audio and camera cut handover.
"""

content += "\n" + new_log

with open('AGENTS.md', 'w', encoding='utf-8') as f:
    f.write(content)

print("Successfully updated AGENTS.md")
