import os

# Update AGENTS.md
agents_entry = """- **2026-10-26 [Antigravity]**: Resolved Multi-Floor Collision, Door/Camera Overflow, Ghost Door Triggers, Periodic Audio Hiss & Main Hall Staircase:
  1. "shhhh" Noise Elimination: Root cause 1: `main.cpp` was calling `LoadZombieAudio` on `CHAR00.VB`, where Tone 2 is Chris's hurt sigh ("shhhhh"), played periodically by `m_GroanTimer` in `enemy_zombie.cpp`. Root cause 2: `AudioEngine::LoadRoomAudio` was hardcoding `vhOff + 0xA20` for the VAG size table instead of the standard PS1 VAB position (`vbOff - 512`), failing to load room VABs and leaving the zombie audio buffer active. Fixed VAG size table offset, isolated zombie SFX strictly to rooms with active zombies, and suppressed moans when zombies are defeated or far away.
  2. Multi-Floor Collision Partitioning: In `room_manager.cpp`, `ResolveCollision` was colliding the player against all SCA boundaries regardless of floor. In the Main Hall (`ROOM1010.RDT`), boundaries 11, 15, 16, and 18 are 2nd-floor balcony railings (`floor = 0x0200` = 512) above the staircase, forming an invisible barrier at Z = 5000 that blocked access to the stairs. Added `floorMask = (posY < -1000.0f) ? 0x0200 : 0x0100;` and skipped non-matching floor boundaries, as well as shape types 4 & 5 (stairs/ledges), allowing the player to walk directly up to the staircase.
  3. 16-Bit Coordinate Overflow Resolution: `RoomDoor` and `RoomCamSwitch` coordinates were `int16_t` signed integers. Across the game, 990 door triggers and 308 camera switch quads exceed 32,767 and wrapped negative, breaking door interaction and spawning players out of bounds. Upgraded `RoomDoor` (`x, z, w, d, nextX, nextZ`) and `RoomCamSwitch` (`x0..x3, z0..z3`) to `uint16_t`.
  4. SCD Opcode-Aware Door Scanner: Replaced unconstrained linear byte search in `scanScriptDoors` with an authentic opcode-aware stepper (`0x0C`: 26 bytes, `0x0D`: 18 bytes, `0x18`: 26 bytes, `0x1B`: 22 bytes, etc.). Eliminated hundreds of false "ghost doors" (e.g. `w=25600, d=74`) that blocked hallways.
  5. Authentic Main Hall Staircase Interactivity: Integrated seamless bi-directional staircase traversal between 1F Main Hall (`Stage 1 Room 0x01`) and 2F Balcony (`Stage 2 Room 0x03`) with footstep audio and camera cut targeting.
"""

with open("AGENTS.md", "r", encoding="utf-8") as f:
    agents_text = f.read()

if "Multi-Floor Collision, Door/Camera Overflow" not in agents_text:
    agents_text += "\n" + agents_entry
    with open("AGENTS.md", "w", encoding="utf-8", newline="\r\n") as f:
        f.write(agents_text)
    print("Updated AGENTS.md")

# Update PROJECT_PARITY.md
parity_entry = """  - [x] **Task 6.5i (Milestone 9)**: Room Collision Multi-Floor Isolation, 16-Bit Coordinate Overflow Resolution, Authentic Opcode Stepper & Main Hall Staircase: `[DONE: Antigravity]`
    - Diagnosed and eliminated periodic "shhhh" noise: fixed VAG size table offset in VAB parser (`vbOff - 512`), isolated zombie voice groans strictly to rooms with active enemies, and eliminated `CHAR00.VB` groan loading.
    - Resolved staircase blockage and hallway clipping: evaluated `floorMask = (posY < -1000.0f) ? 0x0200 : 0x0100;` in `ResolveCollision` so 2nd-floor balcony railings no longer act as invisible barriers on 1st-floor staircases and corridors.
    - Fixed 16-bit coordinate overflow: upgraded `RoomDoor` and `RoomCamSwitch` coordinate fields from `int16_t` to `uint16_t`, fixing 990 doors and 308 camera switch quads that wrapped negative.
    - Implemented authentic opcode-aware door scanner in `RoomManager::ParseRdt`: eliminated all false ghost doors that blocked hallways.
    - Implemented authentic Main Hall staircase interactivity: seamlessly transitions between 1F (`Stage 1 Room 0x01`) and 2F Balcony (`Stage 2 Room 0x03`).
"""

with open("PROJECT_PARITY.md", "r", encoding="utf-8") as f:
    parity_text = f.read()

if "Task 6.5i (Milestone 9)" not in parity_text:
    parity_text += "\n" + parity_entry
    with open("PROJECT_PARITY.md", "w", encoding="utf-8", newline="\r\n") as f:
        f.write(parity_text)
    print("Updated PROJECT_PARITY.md")
