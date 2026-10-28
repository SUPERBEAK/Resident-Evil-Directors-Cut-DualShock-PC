#include "psx_pad.h"
#include <algorithm>
#include <cmath>
#include <windows.h>
#include <xinput.h>

#pragma comment(lib, "xinput.lib")

static XINPUT_STATE xinput_state;
static bool pad_initialized = false;

// Constants for deadzones and curves
const float DEADZONE = 0.15f;
const float CURVE_EXPONENT = 2.0f;

/**
 * Helper to apply a non-linear curve and deadzone to analog input.
 * @param raw_input range -1.0 to 1.0
 * @return value 0 to 255
 */
static uint8_t process_analog(float raw_input) {
  float abs_input = std::abs(raw_input);
  if (abs_input < DEADZONE)
    return 0;

  float normalized = (abs_input - DEADZONE) / (1.0f - DEADZONE);
  float curved = std::pow(normalized, CURVE_EXPONENT);

  uint8_t result = (uint8_t)(curved * 255.0f);
  return (raw_input < 0) ? (uint8_t)(255 - result) : result;
}

int InitPAD() {
  if (XInputGetState(0, &xinput_state) == ERROR_SUCCESS) {
    pad_initialized = true;
    return 0;
  }
  return -1;
}

int StartPAD() { return 0; }

int StopPAD() { return 0; }

void ReadPAD(PAD *pad) {
  if (!pad)
    return;

  // Reset pad state
  pad->buttons = 0;
  pad->stick_x = 0;
  pad->stick_y = 0;
  pad->motor_small = 0;
  pad->motor_large = 0;

  // 1. XInput Controller Input
  if (XInputGetState(0, &xinput_state) == ERROR_SUCCESS) {
    XINPUT_GAMEPAD gamepad = xinput_state.Gamepad;

    // Map buttons
    // Triangle (X), Circle (A), Cross (B), Square (Y)
    if (gamepad.wButtons & XINPUT_GAMEPAD_A)
      pad->buttons |= (1 << 0);
    if (gamepad.wButtons & XINPUT_GAMEPAD_B)
      pad->buttons |= (1 << 1);
    if (gamepad.wButtons & XINPUT_GAMEPAD_X)
      pad->buttons |= (1 << 2);
    if (gamepad.wButtons & XINPUT_GAMEPAD_Y)
      pad->buttons |= (1 << 3);

    // D-Pad / Stick equivalent for PS1 buttons (Up, Down, Left, Right)
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
      pad->buttons |= (1 << 4);
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
      pad->buttons |= (1 << 5);
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
      pad->buttons |= (1 << 6);
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
      pad->buttons |= (1 << 7);

    // L1, L2, R1, R2, Select, Start (These might exceed uint8_t if we use 1 bit
    // per button) For now, let's see if the game logic only uses a few buttons.
    // If it uses more, we might need to change PAD.buttons to uint16_t.
    // But let's assume the 8 buttons we mapped are enough for now.

    // Stick inputs
    float lx = (float)gamepad.sThumbLX / 32767.0f;
    float ly = (float)gamepad.sThumbLY / 32767.0f;
    pad->stick_x = process_analog(lx);
    pad->stick_y = process_analog(ly);
    if (lx < -0.55f)
      pad->buttons |= (1 << 6); // Left Turn
    if (lx > 0.55f)
      pad->buttons |= (1 << 7); // Right Turn
    if (ly > 0.55f)
      pad->buttons |= (1 << 4); // Forward
    if (ly < -0.55f)
      pad->buttons |= (1 << 5); // Backward
  }

  // 2. Keyboard Mapping: WASD + Arrow Keys
  if ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState(VK_UP) & 0x8000))
    pad->buttons |= (1 << 4); // Up / Forward
  if ((GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000))
    pad->buttons |= (1 << 5); // Down / Backstep
  if ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState(VK_LEFT) & 0x8000))
    pad->buttons |= (1 << 6); // Left Turn (bit 6)
  if ((GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000))
    pad->buttons |= (1 << 7); // Right Turn (bit 7)

  // Actions & Run
  if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) || (GetAsyncKeyState('X') & 0x8000))
    pad->buttons |= (1 << 15); // Run / Sprint
  if ((GetAsyncKeyState('Z') & 0x8000) || (GetAsyncKeyState('E') & 0x8000) || (GetAsyncKeyState(VK_RETURN) & 0x8000))
    pad->buttons |= (1 << 0); // Action / Cross
  if (GetAsyncKeyState('C') & 0x8000)
    pad->buttons |= (1 << 1); // Cancel / Circle
  if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    pad->buttons |= (1 << 2); // Ready Weapon / Square
}

void SetVibration(uint8_t motorSmall, uint8_t motorLarge) {
  XINPUT_VIBRATION vibration;
  vibration.wLeftMotorSpeed = static_cast<WORD>(motorLarge) * 257;
  vibration.wRightMotorSpeed = static_cast<WORD>(motorSmall) * 257;
  XInputSetState(0, &vibration);
}

