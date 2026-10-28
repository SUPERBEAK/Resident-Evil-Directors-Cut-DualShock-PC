#ifndef PSX_PAD_H
#define PSX_PAD_H

#include <stdint.h>

/**
 * PS1 PAD structure representing the state of the controller.
 * This is the structure the game logic expects to read.
 */
typedef struct {
  uint32_t buttons;     // Button bitfield
  uint8_t stick_x;     // Analog stick X (0-255)
  uint8_t stick_y;     // Analog stick Y (0-255)
  uint8_t motor_small; // Vibration motor small (0-255)
  uint8_t motor_large; // Vibration motor large (0-255)
} PAD;

/**
 * LIBPAD function shims.
 * These functions are called by the game's internal logic to handle input.
 */
extern int InitPAD(void);
extern int StartPAD(void);
extern int StopPAD(void);
extern void ReadPAD(PAD *pad);

/**
 * Haptic feedback function.
 * @param small intensity of the small motor (0-255)
 * @param large intensity of the large motor (0-255)
 */
extern void SetVibration(uint8_t motorSmall, uint8_t motorLarge);

#endif // PSX_PAD_H
