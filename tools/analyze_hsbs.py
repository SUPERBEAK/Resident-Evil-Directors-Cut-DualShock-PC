import glob, struct, os

def analyze_sep(path):
    with open(path, 'rb') as f: d = f.read()
    if len(d) < 32: return None, []
    vab_body_off = struct.unpack('<I', d[-12:-8])[0]
    sep_off = struct.unpack('<I', d[-8:-4])[0]
    if sep_off >= len(d) or vab_body_off > len(d) or sep_off >= vab_body_off: return None, []
    sep = d[sep_off:vab_body_off]
    if len(sep) < 19: return None, []
    
    tracks = []
    t0_res = (sep[8] << 8) | sep[9]
    t0_tempo = (sep[10] << 16) | (sep[11] << 8) | sep[12]
    t0_len = struct.unpack('>I', sep[15:19])[0]
    tracks.append((0, t0_res, t0_tempo, 19, min(t0_len, len(sep) - 19)))
    
    next_pos = 19 + t0_len
    sub_idx = 1
    while next_pos + 13 <= len(sep):
        if sep[next_pos:next_pos+2] == b'\xff\x2f':
            next_pos += 3
        if next_pos + 13 > len(sep): break
        s_num = struct.unpack('>H', sep[next_pos:next_pos+2])[0]
        s_res = struct.unpack('>H', sep[next_pos+2:next_pos+4])[0]
        s_tempo = (sep[next_pos+4] << 16) | (sep[next_pos+5] << 8) | sep[next_pos+6]
        s_len = struct.unpack('>I', sep[next_pos+9:next_pos+13])[0]
        evt_start = next_pos + 13
        if evt_start >= len(sep): break
        evt_len = min(s_len, len(sep) - evt_start)
        tracks.append((sub_idx, s_res, s_tempo, evt_start, evt_len))
        next_pos = evt_start + evt_len
        sub_idx += 1
    
    results = []
    for (t_idx, res, tempo, evt_start, evt_len) in tracks:
        s = sep[evt_start : evt_start + evt_len]
        pos, running, t = 0, 0, 0
        note_count = 0
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
            if st == 0xFF:
                pos += 1
                if pos < len(s): pos += s[pos] + 1
            elif cmd == 0x90:
                if pos + 1 < len(s):
                    note, vel = s[pos], s[pos+1]; pos += 2
                    if vel > 0: note_count += 1
                else: break
            elif cmd in (0x80, 0xA0, 0xB0, 0xE0): pos += 2
            elif cmd in (0xC0, 0xD0): pos += 1
        results.append((t_idx, note_count, evt_len))
    return os.path.basename(path), results

for p in sorted(glob.glob('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP*.HSB')):
    name, res = analyze_sep(p)
    if name:
        print(f'{name}: {len(res)} tracks -> {res}')
