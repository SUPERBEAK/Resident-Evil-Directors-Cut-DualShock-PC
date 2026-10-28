with open('port/src/hal/psx_str.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = """            if (hdr->chunkNum + 1 >= hdr->chunkCount || m_FrameBits.size() >= hdr->dataSize) {"""
replace = """            if (hdr->chunkNum + 1 >= hdr->chunkCount) {"""

assert target in text, "target chunk condition not found"
text = text.replace(target, replace, 1)

target_skip = """        if ((padButtons & (1 << 14)) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {"""
replace_skip = """        if ((padButtons & (1 << 0)) || (padButtons & (1 << 14)) ||
            (GetAsyncKeyState(VK_SPACE) & 0x8000) ||
            (GetAsyncKeyState(VK_RETURN) & 0x8000) ||
            (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {"""

assert target_skip in text, "target_skip not found"
text = text.replace(target_skip, replace_skip, 1)

with open('port/src/hal/psx_str.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("psx_str.cpp chunk decode condition and skip hotkeys updated successfully.")
