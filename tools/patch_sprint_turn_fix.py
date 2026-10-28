with open('port/src/game/player_character.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# Replace armed locomotion block
old_armed_block = """      if (m_IsRunning) {
        clipStart = 83;
        clipEnd = 107;
        if (m_CurrentAnimFrame < 83.0f || m_CurrentAnimFrame > 107.0f) {
          m_CurrentAnimFrame = 83.0f;
          prevFrame = 83.0f;
        }
        m_CurrentAnimFrame += 0.78f;
        if (m_CurrentAnimFrame > 107.0f) {
          m_CurrentAnimFrame = 83.0f;
        }
        if ((prevFrame < 90.0f && m_CurrentAnimFrame >= 90.0f) ||
            (prevFrame < 102.0f && m_CurrentAnimFrame >= 102.0f)) {
          g_AudioEngine.PlayFootstep(0.85f);
        }
      } else if (m_IsMoving) {
        clipStart = 35;
        clipEnd = 62;
        if (moveFwd) {
          if (m_CurrentAnimFrame < 35.0f || m_CurrentAnimFrame > 62.0f) {
            m_CurrentAnimFrame = 35.0f;
            prevFrame = 35.0f;
          }
          m_CurrentAnimFrame += 0.38f;
          if (m_CurrentAnimFrame > 62.0f) {
            m_CurrentAnimFrame = 35.0f;
          }
          if ((prevFrame < 42.0f && m_CurrentAnimFrame >= 42.0f) ||
              (prevFrame < 56.0f && m_CurrentAnimFrame >= 56.0f)) {
            g_AudioEngine.PlayFootstep(0.65f);
          }
        } else {
          if (m_CurrentAnimFrame < 35.0f || m_CurrentAnimFrame > 62.0f) {
            m_CurrentAnimFrame = 62.0f;
            prevFrame = 62.0f;
          }
          m_CurrentAnimFrame -= 0.35f;
          if (m_CurrentAnimFrame < 35.0f) {
            m_CurrentAnimFrame = 62.0f;
          }
          if ((prevFrame >= 56.0f && m_CurrentAnimFrame < 56.0f) ||
              (prevFrame >= 42.0f && m_CurrentAnimFrame < 42.0f)) {
            g_AudioEngine.PlayFootstep(0.55f);
          }
        }
      } else if (turnLeft || turnRight) {
        clipStart = 63;
        clipEnd = 82;
        if (m_CurrentAnimFrame < 63.0f || m_CurrentAnimFrame > 82.0f) {
          m_CurrentAnimFrame = 63.0f;
        }
        m_CurrentAnimFrame += 0.22f;
        if (m_CurrentAnimFrame > 82.0f) {
          m_CurrentAnimFrame = 63.0f;
        }
      }"""

new_armed_block = """      if (m_IsRunning) {
        clipStart = 63;
        clipEnd = 82;
        if (m_CurrentAnimFrame < 63.0f || m_CurrentAnimFrame > 82.0f) {
          m_CurrentAnimFrame = 63.0f;
          prevFrame = 63.0f;
        }
        m_CurrentAnimFrame += 0.45f;
        if (m_CurrentAnimFrame > 82.0f) {
          m_CurrentAnimFrame = 63.0f;
        }
        if ((prevFrame < 68.0f && m_CurrentAnimFrame >= 68.0f) ||
            (prevFrame < 78.0f && m_CurrentAnimFrame >= 78.0f)) {
          g_AudioEngine.PlayFootstep(0.85f);
        }
      } else if (m_IsMoving) {
        clipStart = 35;
        clipEnd = 62;
        if (moveFwd) {
          if (m_CurrentAnimFrame < 35.0f || m_CurrentAnimFrame > 62.0f) {
            m_CurrentAnimFrame = 35.0f;
            prevFrame = 35.0f;
          }
          m_CurrentAnimFrame += 0.38f;
          if (m_CurrentAnimFrame > 62.0f) {
            m_CurrentAnimFrame = 35.0f;
          }
          if ((prevFrame < 42.0f && m_CurrentAnimFrame >= 42.0f) ||
              (prevFrame < 56.0f && m_CurrentAnimFrame >= 56.0f)) {
            g_AudioEngine.PlayFootstep(0.65f);
          }
        } else {
          if (m_CurrentAnimFrame < 35.0f || m_CurrentAnimFrame > 62.0f) {
            m_CurrentAnimFrame = 62.0f;
            prevFrame = 62.0f;
          }
          m_CurrentAnimFrame -= 0.35f;
          if (m_CurrentAnimFrame < 35.0f) {
            m_CurrentAnimFrame = 62.0f;
          }
          if ((prevFrame >= 56.0f && m_CurrentAnimFrame < 56.0f) ||
              (prevFrame >= 42.0f && m_CurrentAnimFrame < 42.0f)) {
            g_AudioEngine.PlayFootstep(0.55f);
          }
        }
      } else if (turnLeft || turnRight) {
        clipStart = 35;
        clipEnd = 62;
        if (m_CurrentAnimFrame < 35.0f || m_CurrentAnimFrame > 62.0f) {
          m_CurrentAnimFrame = 35.0f;
        }
        m_CurrentAnimFrame += 0.22f;
        if (m_CurrentAnimFrame > 62.0f) {
          m_CurrentAnimFrame = 35.0f;
        }
      }"""

if old_armed_block not in text:
    print("Error: old_armed_block not found in player_character.cpp!")
else:
    text = text.replace(old_armed_block, new_armed_block)
    with open('port/src/game/player_character.cpp', 'w', encoding='utf-8') as f:
        f.write(text)
    print("player_character.cpp successfully updated with correct sprint and turning ranges!")
