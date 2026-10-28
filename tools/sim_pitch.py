with open('extracted/RE1-Dualshock/SLUS_007.47', 'rb') as f:
    d = f.read()

# Load the pitch table at 0x800b1954
text_start = 0x80010000
off = 0x800b1954 - text_start + 2048
import struct
pitch_table = struct.unpack('<' + 'H'*32, d[off:off+64])
print('Pitch table:', [hex(x) for x in pitch_table[:16]])

def compute_psx_pitch(note, center, shift):
    # note + 60 - center
    v0 = (note + 60 - center) & 0xffffffff
    # signed 32-bit:
    if v0 >= 0x80000000: v0 -= 0x100000000
    # mult by 0x2aaaaaab
    a0 = (v0 << 16) >> 16 # sign extend 16-bit
    prod = a0 * 0x2aaaaaab
    mfhi = (prod >> 32) & 0xffffffff
    if mfhi >= 0x80000000: mfhi -= 0x100000000
    v1 = mfhi >> 1
    v0_sign = 1 if a0 < 0 else 0
    a1 = v1 - (-v0_sign if a0 < 0 else 0) # quotient = octave
    
    # remainder = a0 - a1 * 12
    rem = a0 - a1 * 12
    a2 = (shift >> 3) & 0xff
    if a2 >= 16: a2 = 15
    idx = rem * 16 + a2
    table_val = pitch_table[idx] if idx < len(pitch_table) else 0
    oct_diff = a1 - 5
    if oct_diff > 0:
        spu_pitch = table_val << oct_diff
    else:
        spu_pitch = table_val >> (-oct_diff)
    return spu_pitch, spu_pitch / 4096.0

print('Testing pitch for Tone 5 (center=42, shift=57):')
for n in [28, 42, 56]:
    spu, rel = compute_psx_pitch(n, 42, 57)
    print(f'  Note {n}: SPU pitch = 0x{spu:04x} ({rel:.4f}x, {rel*44100:.1f} Hz)')

print('Testing pitch for Tone 0 (center=78, shift=57):')
for n in [57, 78, 80]:
    spu, rel = compute_psx_pitch(n, 78, 57)
    print(f'  Note {n}: SPU pitch = 0x{spu:04x} ({rel:.4f}x, {rel*44100:.1f} Hz)')
