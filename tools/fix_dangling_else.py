with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

bad = """            } else {
                std::printf("\\n[Door Locked] The door is locked. Requires: %s "
                            "(ID %u)\\n",
                            g_Inventory.GetItemName(hitDoor.keyItem),
                            hitDoor.keyItem);
                g_AudioEngine.PlayDryFire();
                canEnter = false;
              }
            }"""

good = """            }"""

if bad in content:
    content = content.replace(bad, good)
    print('Found and replaced bad block')
else:
    # Use regex
    import re
    pattern = re.compile(r'            \} else \{\s*std::printf\("\\n\[Door Locked\].*?canEnter = false;\s*\}\s*\}', re.DOTALL)
    m = pattern.search(content)
    if m:
        content = pattern.sub('            }', content)
        print('Regex replaced bad block')
    else:
        print('Warning: bad block not found')

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
