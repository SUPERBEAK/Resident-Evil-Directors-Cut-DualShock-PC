import struct, math, wave
import numpy as np

K0 = [0, 60, 115, 98, 122]
K1 = [0, 0, -52, -55, -60]

def decode_spu_adpcm(adpcm_bytes):
    s_1 = 0
    s_2 = 0
    num_blocks = len(adpcm_bytes) // 16
    pcm = []
    loop_start = None
    loop_end = None
    
    for b in range(num_blocks):
        block = adpcm_bytes[b*16:(b+1)*16]
        shift = block[0] & 0x0F
        filter_idx = (block[0] >> 4) & 0x07
        flags = block[1]
        
        if flags & 0x04:
            loop_start = len(pcm)
        if flags & 0x01:
            loop_end = len(pcm) + 28
            
        if flags == 7: # End and mute
            break
            
        k0 = K0[filter_idx] if filter_idx < 5 else 0
        k1 = K1[filter_idx] if filter_idx < 5 else 0
        
        for i in range(2, 16):
            byte_val = block[i]
            nibbles = [byte_val & 0x0F, (byte_val >> 4) & 0x0F]
            for nibble in nibbles:
                sample = nibble if nibble < 8 else nibble - 16
                sample = sample << (12 - shift)
                pred = (s_1 * k0 + s_2 * k1) // 64
                sample = sample + pred
                sample = max(-32768, min(32767, sample))
                pcm.append(sample)
                s_2 = s_1
                s_1 = sample
                
    return np.array(pcm, dtype=np.float32), loop_start, loop_end

def render_hsb(path, max_seconds=120):
    with open(path, 'rb') as f: d = f.read()
    vab_body_off = struct.unpack('<I', d[-12:-8])[0]
    sep_off = struct.unpack('<I', d[-8:-4])[0]
    
    # Parse programs and tones
    num_tones = (sep_off - 512 - 2080) // 32
    tones = []
    for t in range(num_tones):
        off = 2080 + t * 32
        prior, mode, vol, pan, center, shift, min_k, max_k = struct.unpack('<BBBBBBBB', d[off:off+8])
        adsr1, adsr2, prog, vag = struct.unpack('<HHhh', d[off+16:off+24])
        tones.append({
            'prog': prog, 'vag': vag, 'vol': vol, 'pan': pan,
            'center': center, 'shift': shift, 'min_k': min_k, 'max_k': max_k,
            'adsr1': adsr1, 'adsr2': adsr2
        })
        
    vag_table = [struct.unpack('<H', d[sep_off-512+i*2:sep_off-510+i*2])[0] for i in range(256)]
    
    # Decode VAGs
    vags = {}
    body = d[vab_body_off:-12]
    offset = 0
    for v in range(1, 256):
        sz = vag_table[v] * 8
        if sz > 0 and offset + sz <= len(body):
            pcm, l_start, l_end = decode_spu_adpcm(body[offset:offset+sz])
            vags[v] = {'pcm': pcm, 'loop_start': l_start, 'loop_end': l_end}
            offset += sz
            
    # Parse all tracks in SEP
    sep = d[sep_off:vab_body_off]
    tracks = []
    t0_res = (sep[8] << 8) | sep[9]
    t0_tempo = (sep[10] << 16) | (sep[11] << 8) | sep[12]
    t0_len = struct.unpack('>I', sep[15:19])[0]
    tracks.append((0, t0_res, t0_tempo, sep[19:19+t0_len]))
    
    next_pos = 19 + t0_len
    sub_idx = 1
    while next_pos + 13 <= len(sep):
        if sep[next_pos:next_pos+2] == b'\xff\x2f':
            next_pos += 3
        if next_pos + 13 > len(sep): break
        s_res = struct.unpack('>H', sep[next_pos+2:next_pos+4])[0]
        s_tempo = (sep[next_pos+4] << 16) | (sep[next_pos+5] << 8) | sep[next_pos+6]
        s_len = struct.unpack('>I', sep[next_pos+9:next_pos+13])[0]
        evt_start = next_pos + 13
        if evt_start >= len(sep): break
        evt_len = min(s_len, len(sep) - evt_start)
        tracks.append((sub_idx, s_res, s_tempo, sep[evt_start:evt_start+evt_len]))
        next_pos = evt_start + evt_len
        sub_idx += 1
        
    sample_rate = 44100
    max_samples = int(max_seconds * sample_rate)
    mix_left = np.zeros(max_samples, dtype=np.float32)
    mix_right = np.zeros(max_samples, dtype=np.float32)
    
    for (t_idx, res, tempo, s) in tracks:
        if len(s) == 0: continue
        resolution = res if res > 0 else 480
        tempo_usec = tempo if tempo > 0 else 500000
        
        pos = 0
        running = 0
        current_sample = 0
        channels_prog = {i: 0 for i in range(16)}
        channels_vol = {i: 100 for i in range(16)}
        channels_pan = {i: 64 for i in range(16)}
        channels_expr = {i: 127 for i in range(16)}
        
        active_notes = [] # list of active note dicts
        key_voices = {} # (ch, note) -> list of note dicts
        
        while pos < len(s) and current_sample < max_samples:
            delta = 0
            while pos < len(s):
                b = s[pos]; pos += 1
                delta = (delta << 7) | (b & 0x7F)
                if (b & 0x80) == 0: break
                
            delta_sec = (delta * (tempo_usec / resolution)) / 1000000.0
            current_sample += int(delta_sec * sample_rate)
            if current_sample >= max_samples or pos >= len(s): break
            
            st = s[pos]
            if st & 0x80:
                pos += 1
                if st < 0xF0: running = st
            else:
                st = running
                
            cmd = st & 0xF0
            ch = st & 0x0F
            
            if st == 0xFF:
                meta = s[pos]; pos += 1
                if meta == 0x51:
                    tempo_usec = (s[pos] << 16) | (s[pos+1] << 8) | s[pos+2]
                    pos += 3
                elif meta == 0x2F:
                    pos += 1; break
                else:
                    length = s[pos]; pos += 1
                    pos += length
            elif cmd == 0x90:
                note, vel = s[pos], s[pos+1]; pos += 2
                # Key off any existing voices on this key
                for inst in key_voices.get((ch, note), []):
                    if inst['rel_start'] is None:
                        inst['rel_start'] = current_sample
                key_voices[(ch, note)] = []
                
                if vel > 0:
                    prog_id = channels_prog[ch]
                    prog_base = prog_id * 16
                    for t_i in range(16):
                        if prog_base + t_i >= len(tones): break
                        tone = tones[prog_base + t_i]
                        if tone['vag'] > 0 and tone['min_k'] <= note <= tone['max_k']:
                            v_info = vags.get(tone['vag'])
                            if v_info is not None and len(v_info['pcm']) > 0:
                                center_note = tone['center'] + tone['shift'] / 128.0
                                pitch = 2.0 ** ((note - center_note) / 12.0)
                                
                                # Volume and pan
                                gain = (vel / 127.0) * (tone['vol'] / 127.0) * \
                                       (channels_vol[ch] / 127.0) * (channels_expr[ch] / 127.0) * 0.45
                                pan_norm = channels_pan[ch] / 127.0
                                gain_l = gain * math.cos(pan_norm * 1.5707963)
                                gain_r = gain * math.sin(pan_norm * 1.5707963)
                                
                                inst = {
                                    'v_info': v_info,
                                    'pitch': pitch,
                                    'gain_l': gain_l,
                                    'gain_r': gain_r,
                                    'start_sample': current_sample,
                                    'rel_start': None,
                                    'is_looping': v_info['loop_start'] is not None and v_info['loop_end'] is not None
                                }
                                active_notes.append(inst)
                                key_voices[(ch, note)].append(inst)
            elif cmd == 0x80:
                note, vel = s[pos], s[pos+1]; pos += 2
                for inst in key_voices.get((ch, note), []):
                    if inst['rel_start'] is None:
                        inst['rel_start'] = current_sample
                key_voices[(ch, note)] = []
            elif cmd == 0xB0:
                cc, val = s[pos], s[pos+1]; pos += 2
                if cc == 7: channels_vol[ch] = val
                elif cc == 10: channels_pan[ch] = val
                elif cc == 11: channels_expr[ch] = val
            elif cmd == 0xC0:
                channels_prog[ch] = s[pos]; pos += 1
            elif cmd == 0xE0:
                pos += 2
                
        # Now synthesize all active_notes for this track
        # Natural release duration: 0.35s for plucked, 0.75s for sustained
        for inst in active_notes:
            v_info = inst['v_info']
            pcm = v_info['pcm']
            loop_start = v_info['loop_start']
            loop_end = v_info['loop_end']
            is_looping = inst['is_looping']
            pitch = inst['pitch']
            start_s = inst['start_sample']
            rel_s = inst['rel_start'] if inst['rel_start'] is not None else max_samples
            
            # Natural release duration
            rel_dur = int(0.45 * sample_rate) # 450ms smooth release
            end_s = min(rel_s + rel_dur, max_samples)
            if not is_looping:
                raw_dur = int(len(pcm) / pitch)
                end_s = min(end_s, start_s + raw_dur)
                
            if start_s >= max_samples: continue
            
            # Synthesize with linear interpolation and sample looping
            wave_pos = 0.0
            for s_idx in range(start_s, end_s):
                wave_pos = (s_idx - start_s) * pitch
                
                if is_looping and loop_start is not None and loop_end is not None and loop_end > loop_start:
                    if wave_pos >= loop_end:
                        loop_len = loop_end - loop_start
                        wave_pos = loop_start + ((wave_pos - loop_start) % loop_len)
                elif wave_pos >= len(pcm) - 1:
                    break
                    
                idx0 = int(wave_pos)
                if idx0 >= len(pcm): break
                idx1 = idx0 + 1 if idx0 + 1 < len(pcm) else idx0
                frac = wave_pos - idx0
                val = (1.0 - frac) * pcm[idx0] + frac * pcm[idx1]
                
                # ADSR gain
                gain_env = 1.0
                if s_idx >= rel_s:
                    rel_frac = (s_idx - rel_s) / float(rel_dur)
                    # Exponential-like smooth curve
                    gain_env = max(0.0, (1.0 - rel_frac) ** 2)
                    
                mix_left[s_idx] += val * inst['gain_l'] * gain_env
                mix_right[s_idx] += val * inst['gain_r'] * gain_env
                
    # Normalize / clamp
    peak = max(np.max(np.abs(mix_left)), np.max(np.abs(mix_right)))
    scale = 28000.0 / peak if peak > 28000.0 else 1.0
    
    # Trim silence at end
    energy = np.abs(mix_left) + np.abs(mix_right)
    nonzero = np.where(energy > 10.0)[0]
    final_len = nonzero[-1] + 44100 if len(nonzero) > 0 else sample_rate
    final_len = min(final_len, max_samples)
    
    out_left = np.clip(mix_left[:final_len] * scale, -32768, 32767).astype(np.int16)
    out_right = np.clip(mix_right[:final_len] * scale, -32768, 32767).astype(np.int16)
    
    stereo = np.empty((final_len, 2), dtype=np.int16)
    stereo[:, 0] = out_left
    stereo[:, 1] = out_right
    return stereo

stereo_pcm = render_hsb('extracted/RE1-Dualshock/PSXUSA/SOUND/SEP0B.HSB')
with wave.open('temp/test_hq_sep0b.wav', 'wb') as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(44100)
    w.writeframes(stereo_pcm.tobytes())

print(f'Rendered HQ SEP0B: {len(stereo_pcm)} frames ({len(stereo_pcm)/44100:.2f}s)')
