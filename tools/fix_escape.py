with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# Fix any unescaped literal newlines inside printf string literals
replacements = [
    ('checking %zu doors in room\n",', 'checking %zu doors in room\\n",'),
    ('"\n====================================================\n"', '"\\n====================================================\\n"'),
    ('Door slot %u!\n",', 'Door slot %u!\\n",'),
    ('Room 0x%02X\n",', 'Room 0x%02X\\n",'),
    ('Dir: %d\n",', 'Dir: %d\\n",'),
    ('====================================================\n"', '====================================================\\n"'),
    ('facing %.1f deg)\n",', 'facing %.1f deg)\\n",'),
    ('No door in interaction range.\n"', 'No door in interaction range.\\n"')
]

for old, new in replacements:
    text = text.replace(old, new)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print('SUCCESS: Cleaned string escapes in main.cpp')
