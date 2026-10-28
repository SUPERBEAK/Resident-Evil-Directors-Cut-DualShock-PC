with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

depth = 0
for i, line in enumerate(lines):
    stripped = ''
    in_str = False
    for char in line:
        if char == '"': in_str = not in_str
        elif not in_str: stripped += char
    for char in stripped:
        if char == '{': depth += 1
        elif char == '}': depth -= 1
    if i >= 113 and depth == 0:
        print(f"Depth 0 at line {i+1}: {line.strip()}")
print(f"Final depth at end of file: {depth}")
