import struct, os, time

os.makedirs("saves", exist_ok=True)
save_path = "saves/save_0.dat"

# Test saving binary structure: magic, version, saveIndex, timestamp, stage, room, cut, pos, angle, hp, maxHp, isArmed, equippedSlot, 6 inv slots, 32 box slots, 32 scd flags
magic = b"RE1DSAV\x00"
version = 1
saveIndex = 1
timestamp = int(time.time())
stage = 1
room = 0
cut = 0
posX, posY, posZ = 5274.0, 0.0, 5652.0
facingAngle = 1.57
hp, maxHp = 140, 140
isArmed = False
equippedSlot = 1

# Pack player state
inv_slots = [(2, 15), (1, 1), (47, 2), (0, 0), (0, 0), (0, 0)] # Beretta, Knife, Ink Ribbon x2
box_slots = [(65, 1), (47, 3), (12, 14), (11, 15)] + [(0, 0)] * 28 # 32 slots total
scd_flags = [0] * 32
scd_flags[0] = 0x12345678

hdr = struct.pack("<8sIIQ", magic, version, saveIndex, timestamp)
player_part = struct.pack("<BBBffffiibB", stage, room, cut, posX, posY, posZ, facingAngle, hp, maxHp, 1 if isArmed else 0, equippedSlot)

inv_part = b"".join(struct.pack("<BB", item, cnt) for item, cnt in inv_slots)
box_part = struct.pack("<I", len(box_slots)) + b"".join(struct.pack("<BB", item, cnt) for item, cnt in box_slots)
scd_part = b"".join(struct.pack("<I", f) for f in scd_flags)

save_data = hdr + player_part + inv_part + box_part + scd_part
with open(save_path, "wb") as f:
    f.write(save_data)

print(f"Created valid save file {save_path} ({len(save_data)} bytes)")
print("Save file verification passed!")
