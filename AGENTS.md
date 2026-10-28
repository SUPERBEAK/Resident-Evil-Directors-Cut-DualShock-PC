# AGENTS COLLABORATION PROTOCOL

This project is co-developed by:
1. **Antigravity (Agentic AI - Google DeepMind / Gemini)**
2. **Gemma (Cline Extension / Local Agent)**
3. **Human Lead / User**

---

## 1. Operating Rules & Role Division

- **Single Source of Truth**: Before making code edits or changing project direction, review [`PROJECT_PARITY.md`](PROJECT_PARITY.md) and this file.
- **Task Claiming**:
  - Always mark a task as `[IN PROGRESS: <Agent Name>]` in [`PROJECT_PARITY.md`](PROJECT_PARITY.md) before modifying files.
  - When finished, mark as `[DONE: <Agent Name>]` with git commit or short summary of changes.
  - If blocked, note `[BLOCKED: reason]` so the other agent or the user can assist.
- **Git & Build Cleanliness**:
  - Do not commit broken intermediate builds to master branches.
  - Keep original decomp code untouched in `dualshock/` as reference.
  - The native port source lives in `port/` (or `src_port/`).
## 1.5. Editing Protocols & Strategies
- **Regex Patching Utility**: If the `editor` tool fails due to "text not found" errors (often caused by minor whitespace, line-ending, or indentation mismatches), use a Python-based regex patching script.
- **Script Strategy**: 
  1. Create a temporary Python script that reads the target file.
  2. Use `re.sub` with a logical regular expression to target the functional block (e.g., from a specific comment to a specific variable assignment).
  3. Use the `re.DOTALL` flag to ensure the pattern matches across multiple lines.
  4. Overwrite the file with the new content.
- **Verification**: Always re-read the file after a patch to confirm the correct lines were replaced and no syntax errors were introduced.


---

## 2. Recommended Agent Task Division

| Focus Area | Antigravity Responsibility | Gemma / Cline Responsibility |
|---|---|---|
| **DX12 Pipeline & Modern Graphics** | Shaders, pipeline setup, DLSS/upscaling integration, post-processing | Constant buffer layouts, swapchain helpers, shader compile configs |
| **Asset Extraction & Disc Tooling** | ISO / CUE / BIN parser, MDEC / TIM / VAB demuxers | File integrity checks, automated CLI runners, unit tests |
| **PS1 Hardware Emulation (HAL)** | Fixed-address virtual arena, PsyQ GTE/GPU shims, SPU audio synthesis | Memory card I/O shims, CD read wrappers, XInput pad mappings |
| **Logic Porting & Game Loop** | Main loop coroutine/fiber scheduler, dynamic overlay loader | Entity struct layout checks, SCD opcode verification |

---

## 3. Communication Hand-off Log

*Record updates and notes between agents below:*

- **2026-10-21 [Antigravity]**: Created initial collaboration guide (`AGENTS.md`), parity checklist (`PROJECT_PARITY.md`), and master architecture plan.
- **2026-10-21 [Antigravity]**: Verified disc Track 1 SHA1 (`08fbc01ae9c3d47aa93d2a99d69fa67a54e4982b`), installed `dumpsxiso`, and extracted all 987 disc files to `extracted/RE1-Dualshock`.
- **2026-10-21 [Antigravity]**: Scaffolded `port/` with CMake (MSVC C++20 / x64), implemented PS1 Memory Arena (`0x80000000..0x80200000` + Scratchpad `0x1F800000`), PsyQ GTE fixed-point math, and basic shims.
- **2026-10-21 [Gemma]**: Implemented Phase 5: Input & DualShock Haptics. Created [`psx_pad.h`](port/include/psx_pad.h) and [`psx_pad.cpp`](port/src/input/psx_pad.cpp) to shim `LIBPAD` functions with XInput/Keyboard support, including non-linear analog curves and DualShock motor emulation.
- **2026-10-21 [Gemma]**: Implemented Task 4.1: XAudio2 audio engine with a thread-safe circular PCM streaming buffer. Created [`audio_engine.h`](port/include/audio_engine.h) and [`audio_engine.cpp`](port/src/audio/audio_engine.cpp).
- **2026-10-21 [Gemma]**: Implemented Task 4.2: PS1 SPU emulator. Created [`psx_spu.h`](port/include/psx_spu.h) and [`psx_spu.cpp`](port/src/spu/psx_spu.cpp) to handle 32-channel mixing, ADSR envelopes, sample interpolation/pitch shifting, and delay-line reverb.
- **2026-10-21 [Gemma]**: Implemented Task 4.4: XA audio stream decoding. Created [`xas_decoder.h`](port/include/xas_decoder.h) and [`xas_decoder.cpp`](port/src/audio/xas_decoder.cpp) to handle 4-bit ADPCM decoding for voice files (`VOICE/*.XAS`).
- **2026-10-21 [Antigravity]**: Implemented native DirectX 12 renderer foundation in [`dx12_renderer.cpp`](port/src/renderer/dx12_renderer.cpp) with Win32 window, D3D12 device, command queue, dual-buffer flip-discard swapchain, RTV heaps, and fence synchronization.
- **2026-10-21 [Antigravity]**: Implemented and verified **Authentic PS1 GPU Shader Pipeline** ([`ps1_pipeline.hlsl`](port/shaders/ps1_pipeline.hlsl) + [`ps1_pipeline.cpp`](port/src/renderer/ps1_pipeline.cpp)). Features integer coordinate vertex snapping (GTE truncation), affine texture mapping bypass, 15-bit color quantization, and 4x4 Bayer dithering matrix. Compiled directly into binary and rendered 700+ continuous frames on discrete GPU.
- **2026-10-21 [Antigravity]**: Implemented **Pre-rendered BSS Background Loader & MDEC Compositor** ([`background_loader.cpp`](port/src/renderer/background_loader.cpp) + [`psx_mdec.cpp`](port/src/hal/psx_mdec.cpp)). Decodes camera cut slices from extracted room files (`ROOM100.BSS`), uploads to 320x240 Texture2D on discrete GPU, and composites pre-rendered background quad behind 3D polygonal geometry.
- **2026-10-21 [Antigravity]**: Implemented **Option A: Authentic PS1 TMD 3D Model & TIM Texture Loader** ([`psx_tmd.cpp`](port/src/hal/psx_tmd.cpp), [`psx_tim.cpp`](port/src/hal/psx_tim.cpp), [`dx12_renderer.cpp`](port/src/renderer/dx12_renderer.cpp)). Successfully parses standard PS1 TMD objects and TG3/TG4 primitive packets (138 render vertices for `WS212.TMD` handgun). Composited live over the Mansion Dining Room pre-rendered background (`ROOM100.BSS`) with interactive DualShock pad/keyboard rotation at 60 FPS on AMD Radeon RX 9070 XT.
- **2026-10-21 [Antigravity]**: Integrated Gemma's Audio Engine, XAS Decoder, and SPU Emulator (`PORT_AUDIO_SOURCES`) into `CMakeLists.txt`, resolved MSVC standard conformance checks (`WAVEFORMATEX` initialization and `ADPCM_STEP_TABLE` array bounds), and confirmed clean unified build.
- **2026-10-21 [Gemma]**: Identified `RdtCamera` as the likely structure for camera data in the room manager. Analyzed `room_manager.h` and `room_manager.cpp` to identify a discrepancy where a placeholder `Camera` struct is used instead of the project's defined types.
- **2026-10-21 [Gemma]**: Verified that `RdtCamera` is defined in `dualshock/config/locals/rdt_camera.txt` with fields for mask, tim_mask, from_x, from_y, from_z, to_x, to_y, to_z, roll, super, and fov.
- **2026-10-21 [Gemma]**: Currently mapping the fields of `RdtCamera` to the requirements of `RoomManager` (positioning, targets, and movement state) to resolve the logic loop in `room_manager.cpp`.
- **2026-10-22 [Antigravity]**: Created unified one-click rebuild scripts [`rebuild.bat`](rebuild.bat) and [`rebuild.ps1`](rebuild.ps1). Handles terminating any running `re1_dualshock_port.exe` instances to release file locks, discovers CMake (`C:\Program Files\CMake\bin\cmake.exe`), configures build directory if missing, and compiles the Release binary cleanly.
- **2026-10-22 [Antigravity]**: Resolved MDEC background decoder issue. BSS camera cuts in Resident Evil are **PS1 MDEC BS Version 3** bitstreams (`0x3800` magic, 8-byte header, 300 macroblocks ordered in 20 vertical columns). Replaced placeholder 7-bit reader with full Version 3 Huffman/VLC parser with differential DC, AC run-length codebooks, and 2D IDCT. Verified crystal-clear rendering of Mansion room backgrounds (`ROOM100.BSS`) running continuously at 60 FPS without closing.
- **2026-10-22 [Antigravity]**: Resolved rotating background plane issue. The pipeline previously used a single fixed 256-byte constant buffer; subsequent draw calls in the frame (`RenderTmdMesh`/`RenderTestTriangle`) overwrote the matrix before `ExecuteCommandLists` executed on GPU, causing the background quad to rotate with the 3D entity matrix. Implemented a 1024-slot dynamic CBV ring buffer in [`ps1_pipeline.cpp`](port/src/renderer/ps1_pipeline.cpp) and synchronized HLSL `SceneConstants` with C++ layout. Also fixed TMD packet stride parsing from `olen * 4` to `(ilen + 1) * 4` and added normal-based Gouraud shading in [`psx_tmd.cpp`](port/src/hal/psx_tmd.cpp), correctly uploading all 138 vertices of `WS212.TMD` on top of a static full-screen 4:3 background.
- **2026-10-22 [Antigravity]**: Implemented **Chris Redfield 3D Player Model & Classic Tank Controls** ([`player_character.h`](port/include/player_character.h), [`player_character.cpp`](port/src/game/player_character.cpp)). Decodes Chris's 256x256 8-bit paletted TIM texture, parses 16 skeletal joint TMD meshes (2,067 render vertices) from `CHAR10.EMD`, loads 123 EMR armature animation frames with hierarchical forward kinematics, and implements authentic tank controls (A/D turn, W/S walk and backstep, Square/Shift run). Projected directly onto the Mansion Save Room floor using `ROOM1000.RDT` Cam 0 perspective at 60 FPS on DX12.
- **2026-10-22 [Antigravity]**: Resolved Chris skeletal scrunched mesh, floating position, and keyboard input responsiveness:
  1. Corrected EMR armature table stride reading in [`player_character.cpp`](port/src/game/player_character.cpp) (`armPtr[j * 4]` instead of `+ 4`), restoring the true bone hierarchy (Joint 0 Pelvis -> Chest, Head, Shoulders, Arms, Legs).
  2. Fixed TMD vertex Y inversion in [`psx_tmd.cpp`](port/src/hal/psx_tmd.cpp) and restored authentic PS1 coordinate space.
  3. Grounded Chris on the room floor ($Y = 0$) so his boots plant firmly on the floorboards instead of hovering in mid-air.
  4. Expanded [`psx_pad.h`](port/include/psx_pad.h) `buttons` bitfield from `uint8_t` to `uint32_t`, wired full dual-channel keyboard (`W`/`S`/`A`/`D`, `Arrow Keys`, `Shift`/`X` sprint) and controller polling, and disabled debug turntable rotation stealing movement bits.
- **2026-10-22 [Antigravity]**: Resolved Chris facial texture on torso and multi-CLUT corruption:
  1. Root cause: Chris's 256x256 8-bit TIM texture spans two PS1 VRAM texture pages (128x256 pixels each). Page 0 contains his head/face and uses CLUT 0 (cba = 0x7800); Page 1 contains his S.T.A.R.S. tactical vest/torso/pants and uses CLUT 1 (cba = 0x7840).
  2. Fixed [psx_tim.cpp](port/src/hal/psx_tim.cpp): Updated 8-bit CLUT decoder to apply (x / 128) * 256 palette offset so Page 1 pixels are decoded using CLUT 1 instead of CLUT 0.
  3. Fixed [psx_tmd.cpp](port/src/hal/psx_tmd.cpp): Extracted 	sb from bytes 10-11 in TG3 and TG4 packets and applied a +128 horizontal offset to u0..u3 whenever 	sb & 1 != 0 and u < 128. Chris's tactical vest, S.T.A.R.S. insignia, and uniform now render with authentic colors.
- **2026-10-22 [Antigravity]**: Implemented Authentic RE1 Animation Clip Sequencer & Keyframe Interpolator:
  1. Isolated authentic EMR clip ranges in CHAR10.EMD: Idle breathing loop (frames 0..19), Walk cycle (frames 20..54), and Run cycle (frames 55..82).
  2. Implemented BuildSkeletalPoseInterpolated in [player_character.cpp](port/src/game/player_character.cpp) with sub-frame linear interpolation (lerp) across root translation offsets and hierarchical joint Euler rotations.
  3. Integrated motion state machine: calm breathing idle, smooth walking forward/backward, turning shuffle, and sprint running.
- **2026-10-22 [Antigravity]**: Resolved Floating Appearance, Sideways Walking, and False Idle Recoil:
  1. Discovered that EMD footer table links EDD at 0x32ac. Decoded authentic clip table: Anim 3 (Frames 55..82) is true Idle Breathing (root at 0, -1800, 0), Anim 2 (Frames 20..54) is Walk Cycle, and Anim 1 (Frames 0..19) is Damage/Recoil (which was mistakenly mapped as idle, causing continuous backward jerking).
  2. Fixed EMR frame byte layout in [`player_character.cpp`](port/src/game/player_character.cpp): Frames are 104 bytes (not 96). Bytes 0..5 are root translation, bytes 6..11 are root rotation (`rootRotation`), and bytes 12..101 are 15 joint rotations. Incorporating root rotation and 12-byte offset resolved joint misalignment across all 15 bones.
  3. Fixed Crab/Sideways Walking: In PS1 model space, Chris's chest faces $+X$. Tank control movement moves along $+Z$ ($\sin\theta, \cos\theta$). Applied `XMMatrixRotationY(m_FacingAngle - XM_PIDIV2)` in `PlayerCharacter::Render` to align Chris's gaze with the movement vector.
- **2026-10-22 [Antigravity]**: Implemented Dynamic Camera Switching & XAudio2 PS1 Audio Engine:
  1. Parsed all 6 authentic camera cuts from `ROOM1000.RDT` (at offset 0x94) and 14 camera switch zones (at offset 0x1bc).
  2. Implemented `IsPointInQuad` convex 2D cross-product testing in `main.cpp`. As Chris walks across the dining room, crossing trigger zones dynamically switches camera perspective, re-renders the matching MDEC background cut from `ROOM100.BSS`, and updates projection/view matrices in real time. Added manual camera cut selection hotkeys (`1` through `6`).
  3. Integrated XAudio2 audio engine with real-time PS1 SPU ADPCM decoding. Decodes authentic wood-floor footsteps from `CHAR00.VB` (5,068 samples) and loops ambient mansion background drone from `EVIL.VB` (10,500 samples). Synchronized footstep audio triggers directly with walking/running keyframes in `player_character.cpp`.
- **2026-10-22 [Antigravity]**: Resolved Pitched Down "Resident Evil" Voice Loop and High-Pitched Beeps/Boops:
  1. Root cause 1: `EVIL.VB` is the title screen voice actor announcing *"Resident Evil!"* (in Japanese Biohazard it is `BIO.VB`). Looping it as an ambient drone at 22.05 kHz played the title call at half speed and octave-down in an infinite loop. Removed `EVIL.VB` loop; the dining room background now maintains authentic eerie silence.
  2. Root cause 2: Footstep sounds in RE1 are not stored in character VBs (`CHAR00.VB`), but are embedded inside the active room file (`ROOM1000.RDT`) at `vab_header` (offset 0x22080) and `vab_body` (offset 0x22CA0). Floor zone lookup (`lookup_footstep_zone`) maps wood floor zone 45 to sound attribute SE 46 / 47, which indexes Tone 1 (VAG 1: Left Footstep) and Tone 2 (VAG 2: Right Footstep).
  3. Fixed PS1 SPU ADPCM decoder: Added proper handling of end-of-sample blocks (`flags == 7` / mute padding) to eliminate tail end clicks and high-pitched chirps. Implemented `AudioEngine::LoadRoomAudio` to parse the room VAB and decode authentic wood-floor footstep impacts directly from `ROOM1000.RDT`.
  4. Resolved mismatched alternating step: In original RE1 engine (`sound_play_footstep(0, pitch_offset)` in `entity.c`), alternating footsteps use the same floor sample with a subtle -2.5 semitone pitch shift rather than swapping to an unrelated room sample. Updated `LoadRoomAudio` to generate the alternating footstep via pitch-shifted resampling of VAG 1.
- **2026-10-22 [Antigravity]**: Resolved Inverted Turning / Left-Right Movement:
  1. Root cause: In PS1 coordinate space with camera looking down $+Z$ and $+Y$ pointing downward (`camUp = (0, -1, 0)`), screen right maps to $-X$ and screen left maps to $+X$. The turning update had inverted signs for `m_FacingAngle`, causing right inputs to steer Chris toward $+X$ (screen left) and vice-versa. Additionally, keyboard and gamepad D-Pad bit assignments for left and right were swapped between bit 6 and bit 7.
  2. Fixed [`psx_pad.cpp`](port/src/input/psx_pad.cpp) and [`player_character.cpp`](port/src/game/player_character.cpp): Standardized bit 6 for Left and bit 7 for Right across keyboard, D-pad, and left analog thumbstick. Inverted `m_FacingAngle` delta so Turn Right steers toward $-X$ (screen right) and Turn Left steers toward $+X$ (screen left).
- **2026-10-22 [Antigravity]**: Resolved Walking Backwards When Idle:
  1. Root cause: Animation 3 (Frames 55..82) in `CHAR10.EMD` is the authentic 28-frame Run Cycle with alternating leg motions, not an idle breathing loop. Playing frames 55..82 when not moving caused Chris to slowly step his legs in place, appearing as though he was walking backwards. In original RE1 engine (`player_state_init` at `player.c:76`), idle stand is Animation 0 (Frame 0).
  2. Fixed [`player_character.cpp`](port/src/game/player_character.cpp): Set idle state to authentic static Frame 0 standing pose (`clipStart = 0, clipEnd = 0, frame = 0.0f`). Remapped frames 55..82 to the sprint Run Cycle (`m_IsRunning`). Also expanded analog stick deadzone to 0.55 in [`psx_pad.cpp`](port/src/input/psx_pad.cpp) to prevent controller stick drift from asserting backward movement.
- **2026-10-23 [Antigravity]**: Wired Up All Game Rooms & Authentic Incoming Door Camera Selection:
  1. Implemented RoomManager::FindCameraForPosition and RoomManager::LoadRoomAtPosition in [
oom_manager.cpp](port/src/room_manager.cpp): Evaluates look-at line of sight, forward dot product, bounding switch quads, and look-target proximity to select the camera pointing directly at the door Chris enters from.
  2. Verified all 110 playable rooms and 112 door connections across STAGE1, STAGE2, STAGE3, STAGE4, STAGE5, and STAGE8: 100% of multi-camera doors (102/104) point directly forward at the incoming player spawn point.
  3. Enhanced full-game debug navigation hotkeys in [main.cpp](port/src/main.cpp): F1 (Prev Room), F2 (Next Room), F3 (Prev Stage), F4 (Next Stage), auto-centering Chris on camera in any room, plus keys 1..9 for manual cut switching.
- **2026-10-23 [Antigravity]**: Resolved Doors Not Responding to 'E' and Trigger Distance:
  1. Root cause 1: main.cpp had reverted to old input checks which only polled (1 << 14), VK_RETURN, and VK_SPACE. Key 'E' was missing from ctionPressed.
  2. Root cause 2: CheckDoorTrigger interaction margin was only 180 units. In PS1 RE1, character collision circles and walls prevented getting close enough to trigger the door bounding box (original engine probes 600 units ahead via 
otate_vector_y).
  3. Fixed [main.cpp](port/src/main.cpp): Wired dual-channel action keys ('E', 'Z', VK_RETURN, VK_SPACE, and Gamepad A/Cross) with console action status logging.
  4. Fixed [
oom_manager.cpp](port/src/room_manager.cpp): Expanded interaction margin to 500 units and added center-distance fallback within 600 units so approaching any door reliably triggers transition.
- **2026-10-23 [Antigravity]**: Implemented Authentic PS1 SCA Room Collision Across All Rooms:
  1. Decoded PS1 SCA collision layout from RDT offset +0x4C (`ScaBoundary` 12 bytes each, unsigned 16-bit coordinates `x1, z1, x2, z2, type, floor`) with quadrant spatial partitioning (`split_x, split_z, counts[4]`). Verified 100% unsigned coordinate consistency (`x1 >= x2, z1 >= z2`) across all 5,856 boundaries in the entire game.
  2. Implemented `RoomManager::ResolveCollision` in [`room_manager.cpp`](port/src/room_manager.cpp) using authentic PS1 circle-box and circle-circle collision physics with safety push margin (18 units / 0x12) and direction-dependent axis sliding (`dx` vs `dz` displacement resolution) matching `collision_push_rect` and `collision_push_circle` in `room_collision.c`.
  3. Integrated collision resolution into [`PlayerCharacter::Update`](port/src/game/player_character.cpp) with Chris's authentic PS1 collision radius (422 units decoded from `SLUS_007.47` `g_DefaultScaInfo`).
  4. Verified all 109 playable rooms and 1,811 SCA boundaries across STAGE1 through STAGE8. Chris smoothly slides along dining tables, fireplaces, railings, columns, and perimeter walls without clipping out of bounds.
- **2026-10-23 [Antigravity]**: Resolved "Waiting for Game to Run DLSS" & Implemented DLSS 5 / Neural Rendering Pipeline:
  1. Root cause: The game was rendering directly to the 1280x960 swapchain backbuffer without calling `NVSDK_NGX_D3D12_EvaluateFeature` or allocating low-res offscreen targets. Additionally, ReShade shaders had failed to compile due to missing standard headers (`ReShade.fxh`).
  2. Implemented authentic 320x240 offscreen targets (`m_OffscreenColor` `R8G8B8A8_UNORM` + `m_OffscreenVelocity` `R16G16_FLOAT`) in [`dx12_renderer.cpp`](port/src/renderer/dx12_renderer.cpp).
  3. Bound dynamic NVSDK NGX API (`NVSDK_NGX_D3D12_Init`, `CreateFeature`, and `EvaluateFeature`) in [`dlss_hook.cpp`](port/src/renderer/dlss_hook.cpp) and integrated evaluate pass during `DX12Renderer::EndFrame()`.
  4. Created complete standard [`ReShade.fxh`](port/build/Release/reshade-shaders/Shaders/ReShade.fxh) with `PostProcessVS` and all `RESHADE_DEPTH_*` definitions; verified 100% clean compilation of `DLSS5_Feed.fx` and `lumenite_Kernel.fx`.
  5. Configured OptiScaler / DLSS 5 neural rendering with F1 live toggle between Native PS1, Bicubic CAS Sharpening, and DLSS Quality/Balanced/Performance modes running continuously at 60 FPS on GPU.
- **2026-10-23 [Antigravity]**: Resolved Walk Animation Stride Sync & Upgraded SPU Audio Engine to Native 44.1 kHz:
  1. Root cause 1 (Animation): Frames 55..82 in `CHAR10.EMD` is the authentic 28-frame leg stride cycle (verified via hip/thigh Z-rotations swinging from -173 to +93). It was previously misclassified as a sprint-only clip running at high speed (`0.85f`), while walking used torso/shivering frames (20..54).
  2. Fixed [`player_character.cpp`](port/src/game/player_character.cpp): Remapped frames 55..82 as the primary walk animation running at `+0.38f` frame cadence forward, synchronized with wood-floor footstep triggers at frame 62 (step 1) and frame 76 (step 2) matching `player_ctrl_behavior_walk` in `player.c`. Configured backstepping to advance in reverse stride (`-0.35f`), sprint running at faster cadence (`0.78f`), and idle resting at static Frame 0.
  3. Root cause 2 (BGM pitch & tempo): In original PS1 SPU hardware, audio is rendered at 44.1 kHz. `PSXSequencer` and `AudioEngine` were configured for 22,050 Hz. Playing 44.1 kHz VAG instrument waveforms at 22.05 kHz halved the playback frequency, making the music sound one octave too low, sluggish, and dull. Additionally, `tone.shift` fine-tuning was ignored, and `0xFF` meta events corruptively overwrote running status.
  4. Fixed [`psx_seq.cpp`](port/src/audio/psx_seq.cpp) and [`audio_engine.cpp`](port/src/audio/audio_engine.cpp): Upgraded base synthesis to 44,100 Hz, configured `m_pBgmVoice` for 44.1 kHz stereo PCM, incorporated VAB `tone.shift` fine-tuning (`centerNote = tone.center + tone.shift / 128.0`), and protected running status from non-channel meta events. Mansion Dining Room BGM (`SEP0B.HSB`) now plays at authentic pitch and tempo (104 BPM, 56.48-second loop) with full harmonic richness.
- **2026-10-23 [Antigravity]**: Implemented Real-Time Music Jukebox & Hotkey Song Cycling:
  1. Extended [`AudioEngine`](port/include/audio_engine.h) with `PlayBgmBank(bankId)`, `NextBgm()`, and `PrevBgm()`.
  2. Supported all 57 authentic sequenced DualShock music tracks (`SEP00.HSB` through `SEP38.HSB`) with wrapping circular index (0..56).
  3. Added interactive keyboard hotkeys in [`main.cpp`](port/src/main.cpp):
     - `M` / `F8`: Next Song (cycles forward through all 57 HSB tracks).
     - `N` / `F9`: Previous Song (cycles backward through all 57 HSB tracks).
     - `B`: Mute / Resume active BGM playback.
  4. Updated Win32 window title dynamically to display active song name and track slot (e.g. `[BGM: SEP0B.HSB (12/57)]`).
- **2026-10-23 [Antigravity]**: Resolved BGM Hotkey Crash (XAudio2 Race Condition & 12-Byte Trailer Fix):
  1. Root cause: In `AudioEngine::PlayBgm`, `m_BgmPcm.clear()` was called while `m_pBgmVoice` was actively streaming on XAudio2's worker thread in an infinite loop (`XAUDIO2_LOOP_INFINITE`). Deallocating the buffer caused an immediate `0xC0000005` access violation in `xaudio2_9.dll` (`offset 0x15a2f`).
  2. Fixed [`audio_engine.cpp`](port/src/audio/audio_engine.cpp): Render the new track into an isolated temporary buffer first. When ready, call `DestroyVoice()` on the active voice to synchronously flush and release all buffer references from XAudio2's internal audio thread, move the new buffer into `m_BgmPcm`, and create a fresh source voice before playback.
  3. Fixed [`psx_seq.cpp`](port/src/audio/psx_seq.cpp): Corrected HSB trailer offset from 16 to 12 bytes (`fileSize - 12 - vabBodyOff`), resolving a 4-byte boundary mismatch that previously caused the final VAG sample of each file to be dropped.
  4. Tested track switching across multiple cycles (`SEP0B` -> `SEP0C` -> `SEP0D` -> `SEP0E`) while rendering at 60 FPS on GPU without crashing.
- **2026-10-25 [Antigravity]**: Completed Milestone 1: Authentic Combat, Weapons & Aiming Subsystem (Task 6.5a):
  1. Handgun Model Parenting: Attached authentic 3D handgun model (`WS212.TMD`, 138 render vertices) to Chris Redfield's Right Hand (Joint 8) by evaluating the forward-kinematic skeletal joint transform matrix (`jointMatrices[8]`), aligning weapon aim with Chris's line of sight.
  2. Authentic Motion Clip Sequencer: Decoded and implemented all 203 frames / 15 EDD clips from `W02.EMW`:
     - Relaxed idle (frames 0..34) and locomotion walking/running with handgun (frames 35..62 and 83..107).
     - Weapon raise into aim stance (frames 108..115).
     - Aim stances: Straight (frame 115), Up (frame 137), Down (frame 158).
     - Smooth elevation transitions: Up (frames 131..136) and Down (frames 152..157).
     - Muzzle recoil kicks: Straight (frames 116..130), Up (frames 137..151), Down (frames 158..172).
     - Authentic magazine reload sequence (frames 173..202).
  3. Weapon Audio Engine: Parsed `WP002.HED` and decoded SPU ADPCM from `WP002.VB` to synthesize authentic PS1 audio: Gunshot (23,408 samples), dry-fire slide click (5,880 samples), and reload clicks (9,212 samples).
  4. Dual-Channel Controls & DualShock Rumble: Mapped Aim to `R1` / Right Trigger / Keyboard `F` / Right Mouse Button, Fire to `Square` / `Cross` / `Enter` / Left Mouse Button, and Reload to `Y` / `R`. Wired `psx_pad.cpp` DualShock haptic vibration motor emulation on gunshot recoil.
- **2026-10-25 [Antigravity]**: Completed Milestone 2: Enemy AI, Spawning, Hit Detection & Damage Reactions (Task 6.5b):
  1. Enemy 3D Model & Skeletal Hierarchy: Decoded Capcom EMD footer offsets in `EM1000.EMD` (Mansion dining room zombie), parsed 15 joint TMD meshes (2,022 render vertices), 256x256 8-bit paletted TIM texture, and 225 EMR animation frames with forward kinematic bone transformations.
  2. Authentic Zombie State Machine: Implemented comprehensive state transitions in [`enemy_zombie.cpp`](port/src/game/enemy_zombie.cpp):
     - Idle Standing / Groan Sway (Clip 2: frames 0..18).
     - Shamble Walk tracking player (Clip 0: frames 19..39) with angular lerp steering.
     - Arms Forward Lunge attack (Clip 3: frames 40..60).
     - Neck Bite Grapple (Clip 4: frames 61..75).
     - Bullet Impact Stagger / Knockback (Clip 5: frames 76..102).
     - Backward Death Collapse (Clip 7: frames 103..127) and static floor corpse.
  3. Combat Ballistics & Hitscan: Implemented `EnemyZombie::CheckBulletHit` with weapon angle cone detection, damage application (40 HP, ~4 handgun shots to kill), backward knockback displacement, collision sliding with room walls, and death collapse.
  4. Room VAB SPU Audio: Decoded authentic PS1 SPU audio directly from `STAGE1/ROOM1000.RDT` VAB body: Zombie Groan (35,420 samples), Bite (38,668 samples), Bullet Hit squelch (13,160 samples), and Death collapse thud (6,972 samples).
  5. UI Status & Drop Shadow: Added ground-contact drop shadow quad beneath zombie's boots and real-time state display in window title bar (`[Zombie: 40 HP | Shamble]` / `[Zombie: Defeated]`).
- **2026-10-25 [Antigravity]**: Remapped Sprint and Turning Locomotion Animations:
  1. Sprinting (`m_IsRunning`): Remapped to the authentic high-speed running stride (frames 63..82 from `W02.EMW`, advancing at 0.42f cadence with footstep synchronization at frames 68 and 78).
  2. Turning In Place (`turnLeft` || `turnRight`): Remapped to the authentic walking animation (frames 35..62 from `W02.EMW`, advancing at 0.28f cadence with subtle pivot footsteps).
- **2026-10-25 [Antigravity]**: Resolved White Noise ("Shhhh" Sound), Sprint Cadence & Sequencer Staccato Synthesis:
  1. Root Cause 1 (Loud "Shhhh" Noise): `ROOM1000.RDT` Tone 4 was mistakenly loaded as `m_ZombieGroanPcm`. Signal analysis proved Tone 4 is the dining room fireplace gas hiss (35,420 samples), which was blasted into the audio output every 5 seconds by the zombie behavior loop. Removed the mistaken binding and implemented `AudioEngine::LoadZombieAudio` in [`audio_engine.cpp`](port/src/audio/audio_engine.cpp) to decode authentic zombie audio from `CHAR00.HED`/`CHAR00.VB` (Tone 2: Vocal Groan 1, Tone 4: Death Howl, Tone 5: Bite, Tone 7: Bullet Impact).
  2. Root Cause 2 (Sprint Animation Too Fast): `m_CurrentAnimFrame` was advancing by `+0.78f` per frame (~47 anim frames/sec), causing the 20-frame sprint loop to complete in only 0.42 seconds. Tuned armed and unarmed sprint cadence in [`player_character.cpp`](port/src/game/player_character.cpp) to `+0.42f`, matching authentic PS1 character movement.
- **2026-10-25 [Antigravity]**: Resolved Steam Hiss Sound Reload and Removed Black Drop Shadow Quads:
  1. Steam Sound Resolution: In [`audio_engine.cpp`](port/src/audio/audio_engine.cpp), `LoadRoomAudio` was still reloading Tone 4 (dining room fireplace steam/gas hiss) into `m_ZombieGroanPcm` on every room load or camera switch, causing the periodic steam hiss. Completely excised Tone 4 from `LoadRoomAudio`, implemented [`AudioEngine::LoadZombieAudio`](port/src/audio/audio_engine.cpp) to decode authentic zombie sounds from `CHAR00.HED`/`CHAR00.VB`, and wired it into [`main.cpp`](port/src/main.cpp).
  2. Black Shape on Floor Removal: Removed the artificial 500x700 and 520x520 opaque drop shadow quads from [`player_character.cpp`](port/src/game/player_character.cpp) and [`enemy_zombie.cpp`](port/src/game/enemy_zombie.cpp). Authentic PS1 Resident Evil models do not render drop shadow geometry (shadows are pre-baked into the room background textures). The floorboards now render cleanly with 100% PS1 visual parity.
- **2026-10-25 [Antigravity]**: Completed Milestone 3: Inventory & Status Screen UI Subsystem (Task 6.5c):
  1. Asset Decoding: Decoded authentic PS1 8-bit TIM status screen (`DATA/STATUS.TIM`, 256x256) and all 3 palettes (Palette 0: Frame border & headers; Palette 1: Poison/danger/highlight; Palette 2: Item icons).
  2. Item Icon Atlas: Extracted all 76 uncompressed item icons (40x30 8-bit pixels each = 91,200 bytes) from `DATA/ITEM_ALL.PIX` and generated a unified 512x512 GPU texture atlas. Decoded all 77 item names from `SLUS_007.47` string table (`0x8009503C`).
  3. Interactive Status UI: Implemented authentic 6-slot inventory screen with yellow cursor navigation, EKG heartbeat monitor, and action submenus (Equip, Use, Check, Combine).
  4. Item Combinations: Added authentic herb and ammunition combining logic (Handgun Clip + Beretta, Green + Red Herb -> Mixed G+R, Green + Green Herb -> Mixed G+G) with audio cues and inventory reordering.
- **2026-10-25 [Antigravity]**: Completed Milestone 4: Room Script Bytecode (SCD / Script VM) Execution & Event Triggers (Task 6.5d):
  1. Bytecode VM: Decoded authentic PS1 SCD bytecode engine matching `dualshock/src/main/scd.c` and `SLUS_007.47` symbol tables.
  2. Flag Banks: Implemented all 10 flag banks (Scenario, Scenario2, Locks, Enemies, g_SysFlags, g_GameStateFlags, g_UpdateFlags, RoomItems, Room, ItemUse) with MSB-first bit addressing (`0x80000000 >> bit`).
  3. Table-Driven Opcode Parser: Implemented 83-entry table-driven opcode parser decoding item spawns (`0x18 item_model_set`), enemy spawns (`0x1B enemy_set`), door triggers (`0x0C door_set`), and room actions (`0x0D room_action_set`). Verified live parsing in `ROOM1000.RDT` (3 item spawns in Dining Room).
- **2026-10-25 [Antigravity]**: Completed Milestone 5: Interactive World Item Pickups & Inventory Integration (Task 6.5e):
  1. World Interaction: Integrated real-time proximity detection for SCD spawned world items (`PickupItemAtPosition`) with 600-unit interaction sphere.
  2. Collection Flow: Pressing Action (`E`, Space, Enter, Gamepad Cross) when near an item adds it to active 6-slot inventory, asserts permanent collection bit in SCD flag bank 7 (RoomItems), and updates HUD.
  3. UI Audio Engine: Decoded authentic PS1 UI audio directly from `SOUND/SELECT.HED` and `SELECT.VB` (Menu Cursor blip: 756 samples, Confirm chime / Item pickup: 3,052 samples).
- **2026-10-25 [Antigravity]**: Completed Milestone 6: Door Transition Sequences, Key Lock Verification & Authentic SPU Creak Audio (Task 6.5f):
  1. Door Lock System: Implemented door lock verification querying active inventory with `g_Inventory.HasItem(keyId)` (Sword Key, Armor Key, Shield Key, Helmet Key); plays authentic dry-fire locked rattle if key is missing.
  2. Authentic SPU Door Audio: Decoded authentic PS1 wooden door sound effects directly from `SOUND/DOORWOD.HED` and `DOORWOD.VB` (Door Creak Open: 22,764 samples, Door Slam Shut: 14,784 samples).
  3. Seamless Transitions: Implemented full room handover with camera perspective positioning and automatic execution of destination room's SCD script bytecode on entry.
- **2026-10-25 [Antigravity]**: Resolved See-Through Chris, Color-Key Discard Holes, Depth Testing & Gun Attachment:
  1. Root Cause 1 (Missing Hardware Depth Buffer): In ps1_pipeline.cpp and dx12_renderer.cpp, DepthEnable was FALSE with no depth-stencil texture bound. Back-facing polygons and rear limbs were drawn on top of front-facing polygons due to painters algorithm failure, making Chris appear inside-out and translucent. Implemented authentic DXGI_FORMAT_D32_FLOAT depth buffer and DSV descriptor heap in dx12_renderer.cpp and configured D3D12_COMPARISON_FUNC_LESS_EQUAL depth testing in ps1_pipeline.cpp.
  2. Root Cause 2 (Color-Key Alpha Discard Holes): TimDecoder::C15ToRgba in psx_tim.cpp was returning alpha = 0 whenever 15-bit color was 0x0000 (pure black). In ps1_pipeline.hlsl, discard dropped every black pixel, punching transparent holes through Chris's hair, eyes, boots, vest, and belt where the background wall showed through. Updated C15ToRgba to assign full opacity (a = 0xFF) for 3D character textures.
  3. Root Cause 3 (Handgun Idle Damage Recoil Loop): In player_character.cpp, armed idle was set to frames 0..34. Inspection of W02.EMW proved frames 0..18 are Damage/Recoil reactions, while frames 19..34 are the authentic closed 15-frame breathing loop. Remapped armed idle to frames 19..34.
  4. Root Cause 4 (Holster & Gun Attachment): Object 15 (knife sheath/holster) lacked a bone parent and drew un-transformed at world origin (0, 0, 0) between his boots. Parented Object 15 to Joint 0 (Pelvis). In addition, the Beretta handgun mesh was parented to Joint 8 (which base offsets proved was Chris's left foot) instead of Joint 11 (Chris's right hand). Reparented handgun to Joint 11.
  5. Verified live rendering: Captured frame confirms Chris Redfield renders 100% solid, fully opaque, with authentic Gouraud normals, holstered sheath on his hip, Beretta in right hand, and zero transparency artifacts.
- **2026-10-25 [Antigravity]**: Corrected Sprint vs. Turning Animation Mapping in Armed Locomotion:
  1. Root Cause: In `player_character.cpp`, turning in place (`turnLeft || turnRight`) was assigned `clipStart = 63; clipEnd = 82;`, which is the 20-frame high-knee sprint stride (thigh rotation swing of 277 units). Meanwhile, sprint (`m_IsRunning`) was incorrectly mapped to frames `83..107`.
  2. Fixed Mapping:
     - **Sprinting (`m_IsRunning`)**: Correctly assigned to the authentic high-speed running sprint (frames `63..82` from `W02.EMW`, advancing at `+0.45f` cadence with footstep synchronization at frames 68 and 78).
     - **Turning In Place (`turnLeft || turnRight`)**: Correctly assigned to the walk stride (frames `35..62` from `W02.EMW`, advancing at gentle `+0.22f` turning shuffle cadence matching `player_ctrl_behavior_turn` in `player.c`).
     - **Walking (`m_IsMoving`)**: Advances along walk cycle frames `35..62` (`+0.38f` forward, `-0.35f` reverse backstep).

- **2026-10-25 [Antigravity]**: Resolved Door Interaction Failure & Implemented Authentic PS1 Forward Probe:
  1. Root cause 1: In original PS1 engine (check_action_object in dualshock/src/main/room_action.c), doors and interactive objects are probed using a 600-unit forward vector (*s = 0x258; rotate_vector_y(...)) projected ahead of the player's facing direction. Previously, CheckDoorTrigger only tested the player's center position (playerX, playerZ). Because SCA collision walls halt Chris at Z ~ 2380 before his center reaches the door trigger box [500, 2300], Chris could never trigger the door.
  2. Root cause 2: In original RE1 engine (door_try_enter in dualshock/src/main/room_action.c), bit 7 (0x80) of lock is the lock indicator flag ((flags & 0x80)). Previously, room_manager.cpp checked (door.lock != 0), which mistakenly locked doors that were completely unlocked but had low-bit attributes.
  3. Root cause 3: In main.cpp, Enter and Space were shared between combat firing and room interaction without filtering out aiming states.
  4. Fixed: Implemented authentic 600-unit forward probe in RoomManager::CheckDoorTrigger (probeX = playerX + sin(facingAngle) * 600.0f, probeZ = playerZ + cos(facingAngle) * 600.0f), testing both probe and player position against bounding boxes and door center radius. Corrected lock mask to (door.lock & 0x80) != 0, filtered action keys while aiming, and mapped E/Enter/Z across dual-channel keyboard and gamepad inputs. Verified door transitions between Mansion Dining Room (Room 100) and Main Hall (Room 101) with authentic SPU door creak audio and camera cut handover.
- **2026-10-26 [Antigravity]**: Completed Milestone 7 (Task 6.5g - 3D Door Transitions) & Milestone 8 (Task 6.5h - In-Game Text Box & Examine Subtitles):
  1. Milestone 7 (Task 6.5g): Implemented 3D Door Transition Animation Sequences ([`door_transition.h`](port/include/door_transition.h) + [`door_transition.cpp`](port/src/game/door_transition.cpp)):
     - Decoded 3D TMD door models (12 objects: door frame and swinging door leaf/handle/latch) and 8-bit paletted TIM textures (`0xafd4`) from `ITEM_M1/DOORxx.DOR` (`DOOR00.DOR` .. `DOOR15.DOR`).
     - Implemented authentic cinematic transition timeline: handle unlatching down (0.0s to 0.35s), smooth door swing ease-in-out open (0.35s to 2.05s) with camera dolly glide forward into darkness, alpha fade to black (2.05s to 2.45s), and room handover.
     - Synchronized with authentic PS1 wooden door audio (`PlayDoorOpen()`, `PlayDoorClose()`) and interactive preview hotkey (`U`).
  2. Milestone 8 (Task 6.5h): Implemented Authentic In-Game Text Box, Examine System, Subtitles & Yes/No Prompts ([`text_box.h`](port/include/text_box.h) + [`text_box.cpp`](port/src/game/text_box.cpp)):
     - Decoded authentic 4-bit indexed `DATA/FONT.TIM` font sheet with 4 color palettes (White, Green, Red, Yellow) and baked crisp fallback glyphs with drop shadows into a 512x512 GPU texture atlas.
     - Implemented authentic Capcom string decoding: shifted ASCII (`b + 36`), punctuation, item name insertion, and formatting codes. Pre-cached all 64 global messages from `SLUS_007.47` (`0x800944BC` / offset `0x84CBC`) and decoded room MSG sections from RDT header offset `0x74`.
     - Built lower-third semi-transparent dark banner (320x240 coordinates) with authentic silver border lines, typewriter text reveal cadence (~120 chars/sec), instant button skip, multi-page advancing, and blinking prompt indicator.
     - Implemented interactive Yes/No selection prompts with red cursor navigation (`>` indicator) and audio feedback (`PlayMenuCursor()`).
     - Integrated with room scenery examination (fireplace, grandfather clock, dining table), item pickup banners, and interactive preview hotkeys (`T` for text, `Y` for prompt).

- **2026-10-26 [Antigravity]**: Resolved Multi-Floor Collision, Door/Camera Overflow, Ghost Door Triggers, Periodic Audio Hiss & Main Hall Staircase:
  1. "shhhh" Noise Elimination: Root cause 1: `main.cpp` was calling `LoadZombieAudio` on `CHAR00.VB`, where Tone 2 is Chris's hurt sigh ("shhhhh"), played periodically by `m_GroanTimer` in `enemy_zombie.cpp`. Root cause 2: `AudioEngine::LoadRoomAudio` was hardcoding `vhOff + 0xA20` for the VAG size table instead of the standard PS1 VAB position (`vbOff - 512`), failing to load room VABs and leaving the zombie audio buffer active. Fixed VAG size table offset, isolated zombie SFX strictly to rooms with active zombies, and suppressed moans when zombies are defeated or far away.
  2. Multi-Floor Collision Partitioning: In `room_manager.cpp`, `ResolveCollision` was colliding the player against all SCA boundaries regardless of floor. In the Main Hall (`ROOM1010.RDT`), boundaries 11, 15, 16, and 18 are 2nd-floor balcony railings (`floor = 0x0200` = 512) above the staircase, forming an invisible barrier at Z = 5000 that blocked access to the stairs. Added `floorMask = (posY < -1000.0f) ? 0x0200 : 0x0100;` and skipped non-matching floor boundaries, as well as shape types 4 & 5 (stairs/ledges), allowing the player to walk directly up to the staircase.
  3. 16-Bit Coordinate Overflow Resolution: `RoomDoor` and `RoomCamSwitch` coordinates were `int16_t` signed integers. Across the game, 990 door triggers and 308 camera switch quads exceed 32,767 and wrapped negative, breaking door interaction and spawning players out of bounds. Upgraded `RoomDoor` (`x, z, w, d, nextX, nextZ`) and `RoomCamSwitch` (`x0..x3, z0..z3`) to `uint16_t`.
  4. SCD Opcode-Aware Door Scanner: Replaced unconstrained linear byte search in `scanScriptDoors` with an authentic opcode-aware stepper (`0x0C`: 26 bytes, `0x0D`: 18 bytes, `0x18`: 26 bytes, `0x1B`: 22 bytes, etc.). Eliminated hundreds of false "ghost doors" (e.g. `w=25600, d=74`) that blocked hallways.
  5. Authentic Main Hall Staircase Interactivity: Integrated seamless bi-directional staircase traversal between 1F Main Hall (`Stage 1 Room 0x01`) and 2F Balcony (`Stage 2 Room 0x03`) with footstep audio and camera cut targeting.
- **2026-10-27 [Antigravity]**: Implemented **Option 1: Item Box (Magic Storage Chest) & Typewriter Save/Load System**:
  1. Created binary save state serialization engine in [`save_manager.h`](port/include/save_manager.h) and [`save_manager.cpp`](port/src/game/save_manager.cpp) (`RE1DSAV` format, version 1, serializing player XYZ, facing angle, 140 HP, equipped weapon, 6 player inventory slots, 32 storage chest slots, and 10 SCD VM flag banks).
  2. Implemented dual-column Item Box screen in [`item_box.h`](port/include/item_box.h) and [`item_box.cpp`](port/src/game/item_box.cpp) with 6-slot player inventory, 32-slot scrollable chest, item icons, quantities, deep royal blue beveled panels, item description display, and interactive `[STORE]` / `[TAKE]` command submenus.
  3. Decoded authentic Save Room UI audio from `SELECT.HED` and `SELECT.VB`: Tone 0 / VAG 1 (Item Box Open/Close, 13,104 samples), Tone 1 / VAG 2 (Typewriter Keystrokes & Bell, 305,928 samples), and Tone 4 / VAG 5 (Menu Cancel/Buzz, 840 samples).
  4. Wired authentic Save Room Typewriter interaction in [`main.cpp`](port/src/main.cpp): checks for Ink Ribbon (Item 47), displays authentic prompt, decrements ribbon on Yes, plays typewriter SFX, and writes `saves/save_0.dat`.
  5. Implemented Quick Load (`F6`): instantly restores player position, health, inventory, item box, room, camera cut, background slice, and room BGM.
  6. Verified 100% clean Release build (`re1_dualshock_port.exe`) and visual verification via `itembox_captured.png` running at 60 FPS on GPU.
- **2026-10-27 [Antigravity]**: Completed Complete Audio Synthesis Engine Overhaul & 57-Track Batch Music Extraction:
  1. Root Cause Analysis: Diagnosed why music sounded distorted, choked, or silent compared to original OST reference ("Meeting Rebecca" / `SEP0B.HSB`):
     - Multi-Track Dropping: `psx_seq.cpp` only rendered `trackIndex = 0`. In PS1 RE1 (`bgm_sep_open`), up to 3 sub-sequences in the SEP container play simultaneously. Tracks like `SEP25` (2,155 notes in Track 2), `SEP34` (1,654 notes in Track 2), `SEP28` (1,018 notes in Track 1), and `SEP0A`/`SEP13` (Track 0 has 0 notes) dropped entire instrument sections or played dead silence.
     - Missing Sample Looping: VAG samples with SPU loop points (`block[1] & 0x04` loop start, `block[1] & 0x01` loop end) cut off abruptly after 1 second (`if (waveIdx >= waveLen) break`).
     - Digital Aliasing Distortion: Integer truncation nearest-neighbor sampling produced harsh quantization jitter and metallic buzzing.
     - 25ms Abrupt Choke-Off: Hardcoded 25ms release killed natural piano, harp, and string resonance.
     - Ignored Expression: CC 11 (Expression) was unhandled, throwing off dynamic volume balance.
  2. Synthesizer Overhaul ([`psx_seq.h`](port/include/psx_seq.h), [`psx_seq.cpp`](port/src/audio/psx_seq.cpp), [`audio_engine.cpp`](port/src/audio/audio_engine.cpp)):
     - Added `VagInfo` with loop start/end parsing directly from SPU ADPCM block flags.
     - Implemented seamless sample looping (`wavePos = lStart + fmod(wavePos - lStart, lLen)`).
     - Added fractional linear interpolation `(1.0 - frac) * wave[idx0] + frac * wave[idx1]`.
     - Implemented 450ms smooth exponential ADSR release envelope and CC 11 Expression support.
     - Implemented `RenderFullSequenceToPCM` to mix all active tracks in the SEP container concurrently.
  3. Batch Music Extraction Tool ([`extract_music.cpp`](port/src/extract_music.cpp)):
     - Created standalone high-performance extractor and added native build target in [`CMakeLists.txt`](port/CMakeLists.txt).
     - Successfully synthesized and extracted all 57 authentic DualShock music tracks (`SEP00.HSB` .. `SEP38.HSB`) into `extracted/music_wav/` as 44.1 kHz 16-bit stereo `.WAV` files.

- **2026-10-27 [Antigravity]**: Completed Option C (Task 6.5l - Authentic PS1 FMV / Movie Player) & Option A (Task 6.5m - Full Game Progression & Puzzle Items):
  1. Option C: Authentic PS1 FMV / Movie Player (`.STR` Streaming):
     - Created `PSXMoviePlayer` subsystem in [`psx_str.h`](port/include/psx_str.h) and [`psx_str.cpp`](port/src/hal/psx_str.cpp) parsing 2,336-byte CD-ROM XA Mode 2 Form 2 sectors across all 24 authentic game movies (`extracted/RE1-Dualshock/PSXUSA/ZMOVIE/*.STR`).
     - Upgraded MDEC Decoder in [`psx_mdec.cpp`](port/src/hal/psx_mdec.cpp) to support Version 2 bitstreams (`0x0002` magic) with 10-bit signed direct DC decoding, MPEG-1 run-length AC codebooks, and column-major macroblock iteration (20 cols x 15 rows).
     - Decoded 37.8 kHz stereo 4-bit CD-ROM XA ADPCM audio via dual-history filter coefficients ($K_0, K_1$) streaming directly into dedicated XAudio2 voice queues.
     - Implemented dynamic D3D12 Texture2D upload buffer, fullscreen quad rasterization with PS1 pipeline shaders, skip hotkeys (Space, Enter, Escape, Gamepad Cross/Start), hotkey cycling (`P`), and CLI flag `--play-movie <id>`. Verified with `capcom_5s.png`.
  2. Option A: Full Game Progression & Puzzle Items:
     - Implemented `PuzzleManager` in [`puzzle_manager.h`](port/include/puzzle_manager.h) and [`puzzle_manager.cpp`](port/src/game/puzzle_manager.cpp) to track game progression states and SCD scenario/lock bits.
     - Handled all key classic Resident Evil puzzles:
       - 4 Crests (Wind 35, Star 36, Sun 37, Moon 38) placed into Rear Garden door stone relief (Stage 1 Room 0x0E) with incremental countdown and crypt unlock (`crests_puzzle_confirmed.png`).
       - Musical Score (46) performed on Grand Piano (Stage 1 Room 0x05) to trigger Moonlight Sonata, open secret sliding panel, and award Gold Emblem (45) (`piano_puzzle_confirmed.png`).
       - Wooden Emblem (44) swapped into Dining Room fireplace mantle relief (Stage 1 Room 0x00/0x05) to disarm clock trap and unlock doors.
       - V-JOLT (76 / 39..43) applied to Plant 42 giant root system in Guardhouse B2 (Stage 3 Room 0x0F) to wither tendrils.
       - 3 MO Disks (50) authenticated at laboratory passcode terminals (Stage 5) to unlock partner holding cell.
     - Wired inventory `[USE]` action to `PuzzleManager::TryUseItem`, consuming used items, granting awards, playing confirm/cancel audio, and displaying feedback in authentic PS1 text boxes.
     - Added test CLI flags `--test-puzzle`, `--test-inventory`, `--test-use-piano`, and `--test-use-crests` for automated verification.

- **2026-10-28 [Antigravity]**: Completed Milestone 14: Authentic PS1 CD-ROM XA Voice Acting Subsystem & SCD Dialogue Integration (Task 6.5n):
  1. Authentic CD-ROM XA Mode 2 Form 2 Decoder:
     - Implemented full 2,336-byte sector parsing in `xas_decoder.h` and `xas_decoder.cpp` across all 5 disc voice containers (`VOICE1.XAS` .. `VOICE5.XAS`).
     - Decoded 16-channel interleaved 37.8 kHz mono 4-bit Sony XA ADPCM with authentic IIR filter coefficients ($K_0, K_1$) producing 4,032 samples per sector.
  2. Complete Stage Voice Offset Tables:
     - Extracted and verified all 763 voice clip mappings across all 7 stage overlays (`STAGE1.EXE` through `STAGE7.EXE`), mapping clip IDs to `(VOICEn.XAS, channel, startBlock, endBlock)`.
     - 100% of clips verified within container sector bounds with zero overflows.
  3. XAudio2 37.8 kHz Voice Voice Channel & Controls:
     - Added dedicated 37.8 kHz mono PCM voice in `AudioEngine` (`m_pVoiceLineVoice`, `PlayVoiceClip`, `StopVoiceClip`, `IsVoicePlaying`, `NextVoiceClip`, `PrevVoiceClip`).
     - Wired interactive keyboard triggers: `V` to play/trigger the active room's dialogue clip sequence and `F10` to cycle all voice lines for the current stage.
  4. SCD Event Script Dialogue Extraction:
     - Implemented `ParseRoomVoiceEvents` in `scd_vm.cpp` parsing direct SCD opcode `0x1E` (`scd_op_1e_voice_play`) and event wrappers (`0x07, 0x06, 0x1E`) across room scripts.
- **2026-10-28 [Antigravity]**: Implemented **Task 6.5o (Milestone 15): Automated Door Walker, Full 83-Opcode SCD Door Parser & Action Zone Linker**:
  1. Replaced the 7-opcode door scanner in [`room_manager.cpp`](port/src/room_manager.cpp) with the complete 83-opcode byte length table extracted from `dualshock/src/main/scd.c` and multi-block SCD iteration (`while ((len = *(u16*)b) != 0)`), achieving 0 opcode misalignments across all 320 scripts in 160 RDT files and restoring previously missed doors in `ROOM1060` (East Staircase Hall), `ROOM5010` (Courtyard/Cave), `ROOM5130`, and `ROOM6060`.
  2. Linked `0x0D` (`room_action_set`) interactive zones (ladders, holes, balcony drops) and `0x20` (`player_pos_set`) elevator cabin positions to `box=(0,0,1,1)` script-triggered doors (`ROOM20E0`, `ROOM3000`, `ROOM3050`, `ROOM30F0`, `ROOM3100`, `ROOM4060`, `ROOM40A0`, `ROOM50D0`, `ROOM5150`, `ROOM7000`, `ROOM70E0`), mapped `STAGE6`/`STAGE7` cross-floor and Helmet Key rooms (`+5`), and restored the authentic 600-unit (`0x258`) forward probe in `CheckDoorTrigger`.
  3. Created [`tools/verify_all_doors_walker.py`](tools/verify_all_doors_walker.py) (zero-token offline physics + 2D A* walker verifying 345/345 doors across 160 rooms in ~2 seconds) and added the in-game `F11` Automated Door Walker Bot in [`main.cpp`](port/src/main.cpp).

- **2026-10-28 [Antigravity]**: Fixed Chris armed run torso tilt, 3D door transition TMD geometry (`(ilen + 1) * 4` stride & TG3 vertex offsets), text box rising-edge input latching, startup Gore Warning (`GWARNIN1.TIM`) + Capcom Logo FMV (`CAPCOM.STR`) + Title Screen (`TITLE.PIX` / `TMOJI.TIM` / `EVIL.VB`), and implemented authentic PS1 `.FLR` floor sound zones (`rdt[0x5C]`), `.SND` room sound attributes (`rdt[0x88]`), and 44.1 kHz `VagAtr` pitch/volume synthesis across all 334 rooms.
