with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace any malformed newlines inside string literals in the action block
content = content.replace('std::printf(\n                "\n====================================================\n");',
                          'std::printf("\\n====================================================\\n");')

content = content.replace('std::printf("[Item Pickup] Collected %s (Item ID %u)!\n", itemName,\n                        pickedItemId);',
                          'std::printf("[Item Pickup] Collected %s (Item ID %u)!\\n", itemName, pickedItemId);')

content = content.replace('std::printf(\n                "====================================================\n\n");',
                          'std::printf("====================================================\\n\\n");')

content = content.replace('std::printf("\n[Item Pickup] Inventory Full! Cannot collect %s\n",\n                        itemName);',
                          'std::printf("\\n[Item Pickup] Inventory Full! Cannot collect %s\\n", itemName);')

content = content.replace('std::printf("\n[Door Unlock] Unlocked door using %s!\n",\n                            g_Inventory.GetItemName(hitDoor.keyItem));',
                          'std::printf("\\n[Door Unlock] Unlocked door using %s!\\n", g_Inventory.GetItemName(hitDoor.keyItem));')

content = content.replace('std::printf("\n[Door Locked] The door is locked from the other side.\n");',
                          'std::printf("\\n[Door Locked] The door is locked from the other side.\\n");')

content = content.replace('std::printf("\n[Door Locked] The door is locked. Requires: %s "\n                            "(ID %u)\n",\n                            g_Inventory.GetItemName(hitDoor.keyItem),\n                            hitDoor.keyItem);',
                          'std::printf("\\n[Door Locked] The door is locked. Requires: %s (ID %u)\\n", g_Inventory.GetItemName(hitDoor.keyItem), hitDoor.keyItem);')

# Also fix any remaining isolated raw newlines in string quotes in that section
import re
def fix_broken_strings(s):
    # Match any printf where the format string has raw newlines
    def repl(m):
        fmt = m.group(1).replace('\r\n', '\\n').replace('\n', '\\n')
        rest = m.group(2)
        return f'std::printf("{fmt}"{rest}'
    return re.sub(r'std::printf\("([^"]*?\n[^"]*?)"([^;]*?;)', repl, s, flags=re.DOTALL)

for _ in range(5):
    content = fix_broken_strings(content)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Successfully cleaned main.cpp string literals!')
