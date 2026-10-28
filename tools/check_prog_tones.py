import glob, struct, os

def check_vab_tones(path):
    with open(path, 'rb') as f: d = f.read()
    vab_body_off = struct.unpack('<I', d[-12:-8])[0]
    sep_off = struct.unpack('<I', d[-8:-4])[0]
    vag_tbl_off = sep_off - 512
    tone_bytes = vag_tbl_off - 2080
    num_tones = tone_bytes // 32
    
    # Read programs (128 programs * 16 bytes at offset 32)
    progs = []
    for p in range(128):
        off = 32 + p * 16
        num_t = d[off]
        mvol = d[off+1]
        progs.append(num_t)
        
    sum_prog_tones = sum(progs)
    
    # Check tones prog fields
    tone_progs = []
    for t in range(num_tones):
        off = 2080 + t * 32
        prog, vag = struct.unpack('<hh', d[off+20:off+24])
        tone_progs.append((prog, vag))
        
    return os.path.basename(path), num_tones, sum_prog_tones, progs[:10], tone_progs[:10]

for p in sorted(glob.glob('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP*.HSB')):
    name, n_tones, sum_t, p_first, t_first = check_vab_tones(p)
    # Check if tone index = prog * 16 or consecutive
    print(f'{name}: num_tones={n_tones}, sum(prog.tones)={sum_t}')
