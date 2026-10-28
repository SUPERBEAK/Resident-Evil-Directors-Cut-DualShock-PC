import re

with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# Fix broken multi-line strings:
# e.g. std::printf("\n[Door Unlock] ...
text = text.replace('std::printf(" \n', 'std::printf("\\n')
text = text.replace('std::printf("\n', 'std::printf("\\n')
text = text.replace('\n");', '\\n");')
text = text.replace('Cut %u\n",', 'Cut %u\\n",')
text = text.replace('Cut %u\r\n",', 'Cut %u\\n",')
text = text.replace('Game::g_TextBox.ShowMessage(\n                    "\n', 'Game::g_TextBox.ShowMessage("\\n')
text = text.replace('Game::g_TextBox.ShowMessage(\r\n                    "\r\n', 'Game::g_TextBox.ShowMessage("\\n')
text = text.replace('std::printf(\n                    "\\n', 'std::printf("\\n')
text = text.replace('std::printf(\r\n                    "\\n', 'std::printf("\\n')
text = text.replace('std::printf("\r\n[', 'std::printf("\\n[')
text = text.replace('std::printf("\n[', 'std::printf("\\n[')
text = text.replace('using %s!\r\n",', 'using %s!\\n",')
text = text.replace('using %s!\n",', 'using %s!\\n",')
text = text.replace('deg)\r\n",', 'deg)\\n",')
text = text.replace('deg)\n",', 'deg)\\n",')
text = text.replace('Door %u!\r\n",', 'Door %u!\\n",')
text = text.replace('Door %u!\n",', 'Door %u!\\n",')
text = text.replace('0x%02X\r\n",', '0x%02X\\n",')
text = text.replace('0x%02X\n",', '0x%02X\\n",')
text = text.replace('Dir: %d\r\n",', 'Dir: %d\\n",')
text = text.replace('Dir: %d\n",', 'Dir: %d\\n",')
text = text.replace('Camera Cut %u\r\n",', 'Camera Cut %u\\n",')
text = text.replace('Camera Cut %u\n",', 'Camera Cut %u\\n",')
text = text.replace('inside.", false);', 'inside.", false);')
text = text.replace('sword.", false);', 'sword.", false);')
text = text.replace('armor.", false);', 'armor.", false);')
text = text.replace('shield.", false);', 'shield.", false);')
text = text.replace('helmet.", false);', 'helmet.", false);')
text = text.replace('glowing.", false);', 'glowing.", false);')
text = text.replace('stopped.", false);', 'stopped.", false);')
text = text.replace('placed.", false);', 'placed.", false);')
text = text.replace('items.", false);', 'items.", false);')

# Also fix any remaining multiline string literals using regex
def fix_multiline_strings(code):
    lines = code.split('\n')
    out = []
    in_quote = False
    cur_line = ""
    for l in lines:
        l_strip = l.rstrip('\r')
        quote_count = l_strip.count('"') - l_strip.count('\\"')
        if in_quote:
            cur_line += "\\n" + l_strip.lstrip()
            if quote_count % 2 == 1:
                in_quote = False
                out.append(cur_line)
                cur_line = ""
        else:
            if quote_count % 2 == 1:
                in_quote = True
                cur_line = l_strip
            else:
                out.append(l_strip)
    if cur_line:
        out.append(cur_line)
    return '\n'.join(out)

text = fix_multiline_strings(text)

with open('port/src/main.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)

print("String literals fixed successfully.")
