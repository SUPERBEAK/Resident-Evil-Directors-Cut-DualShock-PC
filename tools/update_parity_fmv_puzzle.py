with open('PROJECT_PARITY.md', 'r', encoding='utf-8') as f:
    text = f.read()

entries = """  - [x] **Task 6.5l (Milestone 12)**: Authentic PS1 FMV / Movie Player (`.STR` Streaming & MDEC V2): `[DONE: Antigravity]`
    - Created `PSXMoviePlayer` subsystem in [`psx_str.h`](port/include/psx_str.h) and [`psx_str.cpp`](port/src/hal/psx_str.cpp) parsing 2,336-byte CD-ROM XA Mode 2 Form 2 sectors across all 24 authentic game movies (`extracted/RE1-Dualshock/PSXUSA/ZMOVIE/*.STR`).
    - Upgraded MDEC Decoder in [`psx_mdec.cpp`](port/src/hal/psx_mdec.cpp) to support Version 2 bitstreams (`0x0002` magic) with 10-bit signed direct DC decoding, MPEG-1 run-length AC codebooks, and column-major macroblock iteration (20 cols x 15 rows).
    - Decoded 37.8 kHz stereo 4-bit CD-ROM XA ADPCM audio via dual-history filter coefficients ($K_0, K_1$) streaming directly into dedicated XAudio2 voice queues.
    - Implemented dynamic D3D12 Texture2D upload buffer, fullscreen quad rasterization with PS1 pipeline shaders, skip hotkeys (Space, Enter, Escape, Gamepad Cross/Start), hotkey cycling (`P`), and CLI flag `--play-movie <id>`. Verified with `capcom_5s.png`.

  - [x] **Task 6.5m (Milestone 13)**: Full Game Progression & Puzzle Items (Crests, Emblems, Sheet Music, V-JOLT, MO Disks): `[DONE: Antigravity]`
    - Implemented `PuzzleManager` in [`puzzle_manager.h`](port/include/puzzle_manager.h) and [`puzzle_manager.cpp`](port/src/game/puzzle_manager.cpp) tracking progression states and SCD scenario/lock bits.
    - 4 Crests (Wind 35, Star 36, Sun 37, Moon 38) placed into Rear Garden door stone relief (Stage 1 Room 0x0E) with incremental countdown and crypt unlock (`crests_puzzle_confirmed.png`).
    - Musical Score (46) performed on Grand Piano (Stage 1 Room 0x05) to trigger Moonlight Sonata, open secret sliding panel, and award Gold Emblem (45) (`piano_puzzle_confirmed.png`).
    - Wooden Emblem (44) swapped into Dining Room fireplace mantle relief (Stage 1 Room 0x00/0x05) to disarm clock trap and unlock doors.
    - V-JOLT (76 / 39..43) applied to Plant 42 giant root system in Guardhouse B2 (Stage 3 Room 0x0F) to wither tendrils.
    - 3 MO Disks (50) authenticated at laboratory passcode terminals (Stage 5) to unlock partner holding cell.
    - Wired inventory `[USE]` action to `PuzzleManager::TryUseItem`, consuming used items, granting awards, playing confirm/cancel audio, and displaying feedback in authentic PS1 text boxes.
    - Added test CLI flags `--test-puzzle`, `--test-inventory`, `--test-use-piano`, and `--test-use-crests` for automated verification.
"""

if 'Task 6.5l (Milestone 12)' not in text:
    text += '\n' + entries
    with open('PROJECT_PARITY.md', 'w', encoding='utf-8') as f:
        f.write(text)
    print("PROJECT_PARITY.md updated successfully.")
else:
    print("Entries already exist in PROJECT_PARITY.md.")
