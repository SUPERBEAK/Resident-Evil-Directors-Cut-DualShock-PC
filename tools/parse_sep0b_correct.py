import struct

with open('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP0B.HSB', 'rb') as f:
    d = f.read()

vab_body_off = struct.unpack('<I', d[-12:-8])[0]
sep_off = struct.unpack('<I', d[-8:-4])[0]
sep = d[sep_off:vab_body_off]

t0_len = struct.unpack('>I', sep[15:19])[0]
s = sep[19:19+t0_len]

# Let's parse all events correctly
pos = 0
running = 0
t = 0
events = []
ch_prog = {i: 0 for i in range(16)}

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
        if st < 0xF0:
            running = st
    else:
        st = running
        
    cmd = st & 0xF0
    ch = st & 0x0F
    
    if st == 0xFF:
        meta = s[pos]; pos += 1
        if meta == 0x51:
            tempo = (s[pos] << 16) | (s[pos+1] << 8) | s[pos+2]
            pos += 3
            events.append((t, 'Tempo', tempo))
        elif meta == 0x2F:
            pos += 1 # 00
            events.append((t, 'EndTrack', 0))
            break
        else:
            length = s[pos]; pos += 1
            pos += length
    elif cmd == 0x90:
        note, vel = s[pos], s[pos+1]; pos += 2
        events.append((t, 'NoteOn' if vel > 0 else 'NoteOff', ch, ch_prog[ch], note, vel))
    elif cmd == 0x80:
        note, vel = s[pos], s[pos+1]; pos += 2
        events.append((t, 'NoteOff', ch, ch_prog[ch], note, vel))
    elif cmd == 0xB0:
        cc, val = s[pos], s[pos+1]; pos += 2
        events.append((t, 'CC', ch, cc, val))
    elif cmd == 0xC0:
        prog = s[pos]; pos += 1
        ch_prog[ch] = prog
        events.append((t, 'Prog', ch, prog))
    elif cmd == 0xE0:
        val = (s[pos+1] << 7) | s[pos]; pos += 2
        events.append((t, 'PitchBend', ch, val))

print(f'Total parsed events: {len(events)}')
for e in events[:40]:
    print(e)
