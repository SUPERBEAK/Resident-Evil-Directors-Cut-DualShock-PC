import struct

with open('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP00.HSB', 'rb') as f:
    d = f.read()

vab_body_off = struct.unpack('<I', d[-12:-8])[0]
sep_off = struct.unpack('<I', d[-8:-4])[0]
sep = d[sep_off:vab_body_off]

print(f'SEP total bytes: {len(sep)}')
# Search for end of track marker 0xFF 0x2F
for i in range(len(sep) - 2):
    if sep[i] == 0xFF and sep[i+1] == 0x2F:
        print(f'End of track at offset {i} (0x{i:x}): following bytes: {[hex(x) for x in sep[i:i+20]]}')
