with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

def unpack_c(s):
    out = []
    in_str = False
    escape = False
    i = 0
    while i < len(s):
        c = s[i]
        if escape:
            out.append(c)
            escape = False
            i += 1
            continue
        if c == '\\' and in_str:
            out.append(c)
            escape = True
            i += 1
            continue
        if c == '"':
            in_str = not in_str
            out.append(c)
            i += 1
            continue
        if not in_str and s[i:i+2] == '\\n':
            out.append('\n')
            i += 2
            continue
        out.append(c)
        i += 1
    return ''.join(out)

unpacked = unpack_c(text)
print('Old line count:', len(text.splitlines()))
print('New line count:', len(unpacked.splitlines()))
with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(unpacked)
print('Wrote unpacked main.cpp')
