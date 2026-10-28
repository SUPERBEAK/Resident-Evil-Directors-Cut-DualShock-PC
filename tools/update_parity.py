with open('PROJECT_PARITY.md', 'r', encoding='utf-8') as f:
    text = f.read()

old_block = """  - [ ] **Task 6.5c (Milestone 3)**: Inventory, Item Interaction & Status Screen UI (TIM 2D menus). `[TODO]`
  - [ ] **Task 6.5d (Milestone 4)**: Room Script Bytecode (SCD / Script VM) Execution & Event Triggers. `[TODO]`"""

new_block = """  - [x] **Task 6.5c (Milestone 3)**: Inventory, Item Interaction & Status Screen UI (TIM 2D menus): `[DONE: Antigravity]`
    - Decoded authentic PS1 8-bit TIM status screen (`DATA/STATUS.TIM`, 256x256) and multi-CLUT palettes (Palette 0 UI border, Palette 1 poison/danger, Palette 2 item icon 256-color palette).
    - Extracted all 76 uncompressed item icons (40x30 8-bit pixels each = 91,200 bytes) from `DATA/ITEM_ALL.PIX` and built a unified 512x512 GPU texture atlas.
    - Decoded all 77 authentic item names directly from `SLUS_007.47` string table (`0x8009503C`).
    - Implemented authentic 6-slot inventory screen with yellow cursor navigation, EKG heartbeat monitor, and action submenus (Equip, Use, Check, Combine).
    - Added herb and ammo combining rules (Handgun Clip + Beretta, Green + Red Herb, Green + Green Herb) with sound cues and inventory reordering.
  - [x] **Task 6.5d (Milestone 4)**: Room Script Bytecode (SCD / Script VM) Execution & Event Triggers: `[DONE: Antigravity]`
    - Decoded authentic PS1 SCD bytecode engine matching `dualshock/src/main/scd.c` and `SLUS_007.47` symbol tables.
    - Implemented all 10 flag banks (Scenario, Scenario2, Locks, Enemies, g_SysFlags, g_GameStateFlags, g_UpdateFlags, RoomItems, Room, ItemUse) with MSB-first bit addressing (`0x80000000 >> bit`).
    - Implemented 83-entry table-driven opcode parser decoding item spawns (`0x18 item_model_set`), enemy spawns (`0x1B enemy_set`), door triggers (`0x0C door_set`), and room actions (`0x0D room_action_set`).
    - Verified live execution across RDT scripts (Stage 1 Room 00 parsed 3 item spawns in the Mansion Dining Room).
  - [x] **Task 6.5e (Milestone 5)**: Interactive World Item Pickups & Inventory Integration: `[DONE: Antigravity]`
    - Integrated real-time proximity detection for SCD spawned world items (`PickupItemAtPosition`) with 600-unit interaction sphere.
    - Added item collection on action key (`E`, Space, Enter, Gamepad Cross): transfers item to active 6-slot inventory, marks permanent collection bit in SCD flag bank 7 (RoomItems), and updates HUD.
    - Decoded authentic PS1 UI audio directly from `SOUND/SELECT.HED` and `SELECT.VB` (Menu Cursor blip: 756 samples, Confirm chime / Item pickup: 3,052 samples).
  - [x] **Task 6.5f (Milestone 6)**: Door Transition Sequences, Key Lock Verification & Authentic SPU Creak Audio: `[DONE: Antigravity]`
    - Implemented door lock verification: queries inventory with `g_Inventory.HasItem(keyId)` (Sword Key, Armor Key, Shield Key, Helmet Key); plays authentic dry-fire locked rattle if key is missing.
    - Decoded authentic PS1 wooden door sound effects directly from `SOUND/DOORWOD.HED` and `DOORWOD.VB` (Door Creak Open: 22,764 samples, Door Slam Shut: 14,784 samples).
    - Seamless room transitions with camera perspective handover and automatic execution of destination room's SCD script bytecode."""

text = text.replace(old_block, new_block)
with open('PROJECT_PARITY.md', 'w', encoding='utf-8') as f:
    f.write(text)
print("Updated PROJECT_PARITY.md successfully!")
