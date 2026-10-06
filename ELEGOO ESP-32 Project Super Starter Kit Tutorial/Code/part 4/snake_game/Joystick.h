#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "snake.h"

#define JOY_X_PIN   35    // Joystick X-axis pin
#define JOY_Y_PIN   34    // Joystick Y-axis pin
#define JOY_BUTTON_PIN 32  // Joystick button pin (adjust for actual hardware)

extern bool isGameOver;    // Game over state flag
bool isJoystickButtonPressed(); // Button detection function

#endif