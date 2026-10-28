import struct

with open('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP0B.HSB', 'rb') as f:
    d = f.read()

vab_body_off = struct.unpack('<I', d[-12:-8])[0]
sep_off = struct.unpack('<I', d[-8:-4])[0]

print(f'Total size: {len(d)}, sep_off: {sep_off}, body_off: {vab_body_off}')
vag_table = [struct.unpack('<H', d[sep_off-512+i*2:sep_off-510+i*2])[0] for i in range(256)]
print('Active VAGs:', [(i, sz*8) for i, sz in enumerate(vag_table) if sz > 0])

num_tones = (sep_off - 512 - 2080) // 32
print(f'Num tones: {num_tones}')
for t in range(num_tones):
    off = 2080 + t * 32
    prior, mode, vol, pan, center, shift, min_k, max_k = struct.unpack('<BBBBBBBB', d[off:off+8])
    adsr1, adsr2, prog, vag = struct.unpack('<HHhh', d[off+16:off+24])
    if vag > 0:
        print(f'Tone {t:2d}: prog={prog:2d}, vag={vag:2d}, vol={vol:3d}, pan={pan:3d}, center={center:3d}, shift={shift:3d}, key={min_k:3d}..{max_k:3d}, adsr1=0x{adsr1:04x}, adsr2=0x{adsr2:04x}')

# Now check loop points in the VAGs
body = d[vab_body_off:-12]
offset = 0
for v in range(1, 256):
    sz = vag_table[v] * 8
    if sz > 0:
        vdata = body[offset:offset+sz]
        offset += sz
        num_blocks = len(vdata) // 16
        loop_start_block = None
        loop_end_block = None
        for b in range(num_blocks):
            blk = vdata[b*16:(b+1)*16]
            flags = blk[1]
            if flags & 0x04:
                loop_start_block = b
            if flags & 0x01:
                loop_end_block = b
                break
            if flags == 7:
                break
        print(f'VAG {v}: size={sz} bytes ({num_blocks} blocks = {num_blocks*28} samples), loop_start={loop_start_block} (sample {loop_start_block*28 if loop_start_block is not None else -1}), loop_end={loop_end_block} (sample {loop_end_block*28 if loop_end_block is not None else -1})')
