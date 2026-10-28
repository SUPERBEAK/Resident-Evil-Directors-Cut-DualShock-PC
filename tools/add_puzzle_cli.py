with open('port/src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = """} else if (strcmp(argv[argi], "--test-puzzle") == 0) {
    g_Inventory.AddItem(35, 1); // Wind Crest
    g_Inventory.AddItem(36, 1); // Star Crest
    g_Inventory.AddItem(37, 1); // Sun Crest
    g_Inventory.AddItem(38, 1); // Moon Crest
    g_Inventory.AddItem(46, 1); // Musical Score
    g_Inventory.AddItem(44, 1); // Wooden Emblem
} else if (strcmp(argv[argi], "--test-door") == 0) {"""

replace = """} else if (strcmp(argv[argi], "--test-puzzle") == 0) {
    g_Inventory.AddItem(35, 1); // Wind Crest
    g_Inventory.AddItem(36, 1); // Star Crest
    g_Inventory.AddItem(37, 1); // Sun Crest
    g_Inventory.AddItem(38, 1); // Moon Crest
    g_Inventory.AddItem(46, 1); // Musical Score
    g_Inventory.AddItem(44, 1); // Wooden Emblem
} else if (strcmp(argv[argi], "--test-inventory") == 0) {
    g_Inventory.ToggleOpen();
} else if (strcmp(argv[argi], "--test-use-piano") == 0) {
    auto res = Game::g_PuzzleManager.TryUseItem(46, 1, 5, 0, 0);
    if (res.handled) {
        if (res.awardItemId > 0) g_Inventory.AddItem(res.awardItemId, res.awardCount);
        Game::g_TextBox.ShowMessage(res.message, false);
    }
} else if (strcmp(argv[argi], "--test-use-crests") == 0) {
    Game::g_PuzzleManager.TryUseItem(35, 1, 0x0E, 0, 0);
    Game::g_PuzzleManager.TryUseItem(36, 1, 0x0E, 0, 0);
    Game::g_PuzzleManager.TryUseItem(37, 1, 0x0E, 0, 0);
    auto res = Game::g_PuzzleManager.TryUseItem(38, 1, 0x0E, 0, 0);
    if (res.handled) {
        Game::g_TextBox.ShowMessage(res.message, false);
    }
} else if (strcmp(argv[argi], "--test-door") == 0) {"""

assert target in text, "target not found"
text = text.replace(target, replace, 1)

with open('port/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Added test-inventory, test-use-piano, and test-use-crests to main.cpp")
