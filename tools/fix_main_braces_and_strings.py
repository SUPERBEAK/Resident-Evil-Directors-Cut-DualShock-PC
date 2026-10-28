with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# 1. Fix line 444-446
for i in range(400, min(500, len(lines))):
    if lines[i].strip() == '}' and lines[i+1].strip() == '}' and lines[i+2].strip() == '}':
        print(f"Found triple brace at {i+1}, removing one")
        lines.pop(i+2)
        break

text = ''.join(lines)

# 2. Fix broken string literals
broken_strings = [
    ('std::printf("  Destination: Stage %u Room 0x%02X\n",', 'std::printf("  Destination: Stage %u Room 0x%02X\\n",'),
    ('std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\n",', 'std::printf("  Spawn Point: (%.1f, %.1f, %.1f), Dir: %d\\n",'),
    ('std::printf("[Door Trigger] Interacting with Door slot %u!\n",', 'std::printf("[Door Trigger] Interacting with Door slot %u!\\n",'),
    ('std::printf("[Action] No item or door in interaction range.\n");', 'std::printf("[Action] No item or door in interaction range.\\n");'),
]

# Write out using binary mode to strictly control bytes
import re
# Replace any std::printf with newlines inside double quotes
def fix_printf(match):
    s = match.group(0)
    # replace unescaped \r\n or \n with \n
    s = s.replace('\r\n', '\\n').replace('\n', '\\n')
    # clean up any double slashes before n
    s = re.sub(r'\\+n', r'\\n', s)
    return s

text = re.sub(r'std::printf\(".*?"\s*[,;]', fix_printf, text, flags=re.DOTALL)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Finished fixing main.cpp")
