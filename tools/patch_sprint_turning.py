with open('port/src/game/player_character.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

target = '''      if (m_IsRunning) {
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
      }'''

replacement = '''      if (m_IsRunning) {
        // Authentic Sprint Run: frames 63..82 (fast running stride)
        clipStart = 63;
        clipEnd = 82;
        if (m_CurrentAnimFrame < 63.0f || m_CurrentAnimFrame > 82.0f) {
          m_CurrentAnimFrame = 63.0f;
          prevFrame = 63.0f;
        }
        m_CurrentAnimFrame += 0.78f;
        if (m_CurrentAnimFrame > 82.0f) {
          m_CurrentAnimFrame = 63.0f;
        }
        if ((prevFrame < 68.0f && m_CurrentAnimFrame >= 68.0f) ||
            (prevFrame < 78.0f && m_CurrentAnimFrame >= 78.0f)) {
          g_AudioEngine.PlayFootstep(0.85f);
        }
      } else if (m_IsMoving) {
        // Authentic Walk Cycle: frames 35..62 (forward/backward)
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
        // Turning in place uses the walking animation: frames 35..62
        clipStart = 35;
        clipEnd = 62;
        if (m_CurrentAnimFrame < 35.0f || m_CurrentAnimFrame > 62.0f) {
          m_CurrentAnimFrame = 35.0f;
          prevFrame = 35.0f;
        }
        m_CurrentAnimFrame += 0.28f;
        if (m_CurrentAnimFrame > 62.0f) {
          m_CurrentAnimFrame = 35.0f;
        }
        if ((prevFrame < 42.0f && m_CurrentAnimFrame >= 42.0f) ||
            (prevFrame < 56.0f && m_CurrentAnimFrame >= 56.0f)) {
          g_AudioEngine.PlayFootstep(0.40f);
        }
      }'''

if target in code:
    code = code.replace(target, replacement)
    with open('port/src/game/player_character.cpp', 'w', encoding='utf-8') as f:
        f.write(code)
    print('SUCCESS: Sprint and Turning animations remapped.')
else:
    print('ERROR: target block not found.')
