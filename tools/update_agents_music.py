with open('AGENTS.md', 'r', encoding='utf-8') as f:
    content = f.read()

entry = """- **2026-10-27 [Antigravity]**: Completed Complete Audio Synthesis Engine Overhaul & 57-Track Batch Music Extraction:
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
"""

if "Completed Complete Audio Synthesis Engine Overhaul" not in content:
    content = content.rstrip() + "\n" + entry
    with open('AGENTS.md', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Successfully appended entry to AGENTS.md")
else:
    print("Entry already present in AGENTS.md")
