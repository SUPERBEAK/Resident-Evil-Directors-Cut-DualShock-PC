import struct, glob, os

def parse_sep_file(path):
    with open(path, 'rb') as f: d = f.read()
    vab_body_off = struct.unpack('<I', d[-12:-8])[0]
    sep_off = struct.unpack('<I', d[-8:-4])[0]
    sep = d[sep_off:vab_body_off]
    
    magic = sep[:4]
    if magic not in (b'pQES', b'SEQp'):
        return None
    
    version = struct.unpack('>H', sep[4:6])[0]
    
    tracks = []
    pos = 6
    while pos + 13 <= len(sep):
        seq_id = struct.unpack('>H', sep[pos:pos+2])[0]
        resolution = struct.unpack('>H', sep[pos+2:pos+4])[0]
        tempo = (sep[pos+4] << 16) | (sep[pos+5] << 8) | sep[pos+6]
        rhythm = (sep[pos+7], sep[pos+8])
        data_size = struct.unpack('>I', sep[pos+9:pos+13])[0]
        
        event_start = pos + 13
        event_end = event_start + data_size
        if event_end > len(sep):
            # Maybe data_size is invalid or file ends
            event_end = len(sep)
            
        tracks.append({
            'seq_id': seq_id,
            'resolution': resolution,
            'tempo': tempo,
            'rhythm': rhythm,
            'data_size': data_size,
            'offset': event_start,
            'data': sep[event_start:event_end]
        })
        pos = event_end
        # In case there's alignment or padding, check
        while pos < len(sep) and sep[pos] == 0:
            pos += 1
            
    return os.path.basename(path), version, tracks

for p in sorted(glob.glob('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP*.HSB')):
    name, ver, tracks = parse_sep_file(p)
    print(f'{name}: ver={ver}, {len(tracks)} tracks:')
    for t in tracks:
        print(f"  Seq {t['seq_id']}: res={t['resolution']}, tempo={t['tempo']} (bpm={60000000/t['tempo'] if t['tempo'] else 0:.1f}), data_len={len(t['data'])}")
