with open('port/src/game/puzzle_manager.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target1 = """    if (itemId >= 35 && itemId <= 38) {
        if (stage == 1 && (room == 0x0E || room == 0x0D || playerZ > 5000.0f)) {"""
replace1 = """    if (itemId >= 35 && itemId <= 38) {
        if (stage == 1 && (room == 0x0E || room == 0x0D || room == 0x0C || playerZ > 4500.0f)) {"""

assert target1 in text, "target1 not found"
text = text.replace(target1, replace1, 1)

target2 = """    if (itemId == 46) {
        if (stage == 1 && room == 0x05) {"""
replace2 = """    if (itemId == 46) {
        if (stage == 1 && (room == 0x05 || room == 0x06 || room == 0x0B)) {"""

assert target2 in text, "target2 not found"
text = text.replace(target2, replace2, 1)

target3 = """    if (itemId == 44) {
        if (stage == 1 && room == 0x00) {"""
replace3 = """    if (itemId == 44) {
        if (stage == 1 && (room == 0x00 || room == 0x05)) {"""

assert target3 in text, "target3 not found"
text = text.replace(target3, replace3, 1)

target4 = """std::string PuzzleManager::CheckPuzzleZone(uint8_t stage, uint8_t room, float playerX, float playerZ) {
    if (stage == 1 && (room == 0x0E || room == 0x0D)) {"""

replace4 = """std::string PuzzleManager::CheckPuzzleZone(uint8_t stage, uint8_t room, float playerX, float playerZ) {
    if (stage == 1 && (room == 0x0E || room == 0x0D || room == 0x0C)) {"""

assert target4 in text, "target4 not found"
text = text.replace(target4, replace4, 1)

target5 = """    if (stage == 1 && room == 0x05) {
        if (m_PianoPlayed) {"""

replace5 = """    if (stage == 1 && (room == 0x05 || room == 0x0B || room == 0x06)) {
        if (m_PianoPlayed) {"""

assert target5 in text, "target5 not found"
text = text.replace(target5, replace5, 1)

target6 = """    if (stage == 1 && room == 0x00) {
        if (m_EmblemSwapped) {"""

replace6 = """    if (stage == 1 && (room == 0x00 || room == 0x05)) {
        if (m_EmblemSwapped) {"""

assert target6 in text, "target6 not found"
text = text.replace(target6, replace6, 1)

with open('port/src/game/puzzle_manager.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("puzzle_manager.cpp room match conditions expanded successfully.")
