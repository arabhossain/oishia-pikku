#include "esp32-hal-gpio.h"
#include "Joystick.h"
#include <Arduino.h>
#include "snake.h"

// -------------------------- Joystick Initialization --------------------------
void initJoystick() {
  pinMode(JOY_X_PIN, INPUT);  // Set joystick X-axis as input
  pinMode(JOY_Y_PIN, INPUT);  // Set joystick Y-axis as input
  pinMode(JOY_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Joystick initialized");
}

// -------------------------- Read Joystick Direction --------------------------
void readJoystick() {
  int xVal = analogRead(JOY_X_PIN);
  int yVal = analogRead(JOY_Y_PIN);
  const int MAX_VAL = 4095;  // ADC maximum value (ESP32 default 12-bit)

  // Only restrict reverse movement, no boundary restriction (allow moving toward boundary)
  if (yVal < DEAD_ZONE && current_dir != 2) {        // Up (don't reverse down)
    current_dir = 0;
  } else if (xVal > MAX_VAL - DEAD_ZONE && current_dir != 3) {  // Right (don't reverse left)
    current_dir = 1;
  } else if (yVal > MAX_VAL - DEAD_ZONE && current_dir != 0) {  // Down (don't reverse up)
    current_dir = 2;
  } else if (xVal < DEAD_ZONE && current_dir != 1) {  // Left (don't reverse right)
    current_dir = 3;
  }
  // Maintain current direction when no input
}

bool isJoystickButtonPressed() {
  // Read button state (detect LOW for pull-up configuration)
  return digitalRead(JOY_BUTTON_PIN) == LOW;
}