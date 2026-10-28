with open('PROJECT_PARITY.md', 'r', encoding='utf-8') as f:
    content = f.read()

old_task43 = "- [x] **Task 4.3**: `SEPxx.HSB` sequencer and VAB soundfont playback. `[DONE: Gemma]`"
new_task43 = """- [x] **Task 4.3**: `SEPxx.HSB` sequencer and VAB soundfont playback. `[DONE: Antigravity]`
  - Upgraded synthesis to native 44.1 kHz stereo PCM with high-precision linear interpolation.
  - Implemented authentic SPU ADPCM sample looping (`block[1] & 0x04` loop start, `block[1] & 0x01` loop end).
  - Implemented multi-track concurrent playback for SEP sequence packages (up to 3 tracks simultaneously).
  - Added 450ms smooth exponential ADSR release envelope and CC 11 Expression volume dynamics."""

content = content.replace(old_task43, new_task43)

new_milestone = """  - [x] **Task 6.5j (Milestone 10)**: PSX Audio Sequencer Synthesis Overhaul & 57-Track Complete Music Extraction: `[DONE: Antigravity]`
    - Diagnosed audio rendering discrepancies matching OST reference ("Meeting Rebecca" / `SEP0B.HSB`): multi-track dropping, missing sample looping, nearest-neighbor aliasing, and 25ms choke-off clicks.
    - Implemented authentic sample looping, linear interpolation, 450ms natural ADSR release curves, CC 11 Expression support, and multi-track sequence mixing in [`psx_seq.cpp`](port/src/audio/psx_seq.cpp).
    - Built and executed standalone native music extraction tool [`extract_music.exe`](port/src/extract_music.cpp), batch-rendering all 57 authentic DualShock music tracks (`SEP00.HSB` .. `SEP38.HSB`) to 44.1 kHz 16-bit stereo `.WAV` files in `extracted/music_wav/`.
"""

if "Task 6.5j" not in content:
    content = content.rstrip() + "\n" + new_milestone

with open('PROJECT_PARITY.md', 'w', encoding='utf-8') as f:
    f.write(content)

print("Updated PROJECT_PARITY.md successfully")
