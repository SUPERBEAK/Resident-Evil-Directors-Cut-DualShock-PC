entry = """- **2026-10-25 [Antigravity]**: Corrected Sprint vs. Turning Animation Mapping in Armed Locomotion:
  1. Root Cause: In `player_character.cpp`, turning in place (`turnLeft || turnRight`) was assigned `clipStart = 63; clipEnd = 82;`, which is the 20-frame high-knee sprint stride (thigh rotation swing of 277 units). Meanwhile, sprint (`m_IsRunning`) was incorrectly mapped to frames `83..107`.
  2. Fixed Mapping:
     - **Sprinting (`m_IsRunning`)**: Correctly assigned to the authentic high-speed running sprint (frames `63..82` from `W02.EMW`, advancing at `+0.45f` cadence with footstep synchronization at frames 68 and 78).
     - **Turning In Place (`turnLeft || turnRight`)**: Correctly assigned to the walk stride (frames `35..62` from `W02.EMW`, advancing at gentle `+0.22f` turning shuffle cadence matching `player_ctrl_behavior_turn` in `player.c`).
     - **Walking (`m_IsMoving`)**: Advances along walk cycle frames `35..62` (`+0.38f` forward, `-0.35f` reverse backstep).
"""
with open('AGENTS.md', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.rstrip() + '\n' + entry

with open('AGENTS.md', 'w', encoding='utf-8') as f:
    f.write(text)
print('AGENTS.md updated!')
