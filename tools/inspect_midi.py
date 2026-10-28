import struct

with open('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP0B.HSB', 'rb') as f:
    d = f.read()

vab_body_off = struct.unpack('<I', d[-12:-8])[0]
sep_off = struct.unpack('<I', d[-8:-4])[0]
sep = d[sep_off:vab_body_off]

t0_res = (sep[8] << 8) | sep[9]
t0_tempo = (sep[10] << 16) | (sep[11] << 8) | sep[12]
t0_len = struct.unpack('>I', sep[15:19])[0]
s = sep[19:19+t0_len]

pos, running, t = 0, 0, 0
events = []
channels_prog = {i: 0 for i in range(16)}

while pos < len(s):
    delta = 0
    while pos < len(s):
        b = s[pos]; pos += 1
        delta = (delta << 7) | (b & 0x7F)
        if (b & 0x80) == 0: break
    t += delta
    if pos >= len(s): break
    st = s[pos]
    if st & 0x80:
        pos += 1
        if st < 0xF0: running = st
    else: st = running
    cmd = st & 0xF0
    ch = st & 0x0F
    if st == 0xFF:
        meta = s[pos]; pos += 1
        length = s[pos]; pos += 1
        meta_data = s[pos:pos+length]; pos += length
        if meta == 0x51:
            tempo = (meta_data[0] << 16) | (meta_data[1] << 8) | meta_data[2]
            print(f't={t}: Set Tempo {tempo}')
        elif meta == 0x2F:
            print(f't={t}: End of Track')
            break
    elif cmd == 0x90:
        note, vel = s[pos], s[pos+1]; pos += 2
        prog = channels_prog[ch]
        events.append((t, 'NoteOn' if vel > 0 else 'NoteOff', ch, prog, note, vel))
    elif cmd == 0x80:
        note, vel = s[pos], s[pos+1]; pos += 2
        prog = channels_prog[ch]
        events.append((t, 'NoteOff', ch, prog, note, vel))
    elif cmd == 0xB0:
        cc, val = s[pos], s[pos+1]; pos += 2
        print(f't={t}: CC ch={ch} cc={cc} val={val}')
    elif cmd == 0xC0:
        prog = s[pos]; pos += 1
        channels_prog[ch] = prog
        print(f't={t}: ProgChange ch={ch} prog={prog}')
    elif cmd == 0xE0:
        pos += 2

print(f'Total events: {len(events)}')
for e in events[:30]:
    print(e)
