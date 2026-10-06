#include "snake.h"
#include <Arduino.h>
#include "score.h"
#include "Joystick.h"

// -------------------------- Reset snake to initial state --------------------------
void resetSnake() {
  // Move snake head to screen center
  snake_x[0] = (OLED_WIDTH - SNAKE_SIZE) / 2;  // (128-4)/2=62
  snake_y[0] = (OLED_HEIGHT - SNAKE_SIZE) / 2; // (64-4)/2=30

  // Reset length to 1 (only head remains)
  snake_length = 1;

  // Clear movement direction (stationary)
  current_dir = -1;

  // Reset blink state
  isShow = true;
  if(!start_game){
    isGameOver=true;
  }
  

  Serial.printf("Snake reset: center(%d,%d), length=%d\n", snake_x[0], snake_y[0], snake_length);
}

// -------------------------- Snake movement (including body following) --------------------------
void moveSnake() {
  if (current_dir == -1) return;  // No movement when no direction

  // Body following: each segment inherits previous segment's position (from tail to head)
  for (int i = snake_length - 1; i > 0; i--) {
    snake_x[i] = snake_x[i - 1];
    snake_y[i] = snake_y[i - 1];
  }

  // Head movement (free movement, no boundary restriction)
  switch (current_dir) {
    case 0:  // Up (Y decreases)
      snake_y[0] -= MOVE_STEP+current_score/5;
      break;
    case 1:  // Right (X increases)
      snake_x[0] += MOVE_STEP+current_score/5;
      break;
    case 2:  // Down (Y increases)
      snake_y[0] += MOVE_STEP+current_score/5;
      break;
    case 3:  // Left (X decreases)
      snake_x[0] -= MOVE_STEP+current_score/5;
      break;
  }

  // Debug: print head coordinates
  // Serial.printf("Snake head: (%d,%d) | Direction: %d\n", snake_x[0], snake_y[0], current_dir);
}

// -------------------------- Check if snake head crosses boundary --------------------------
bool checkSnakeOverBoundary() {
  // Boundary condition: any part of snake head outside screen
  bool xOver = (snake_x[0] < 0) || (snake_x[0] > MAX_SNAKE_X);
  bool yOver = (snake_y[0] < 8) || (snake_y[0] > MAX_SNAKE_Y+2);

  if (xOver || yOver) {
    Serial.println("Snake head out of bounds! Game reset");
    return true;
  }
  return false;
}

// -------------------------- Draw snake (head + body) --------------------------
void drawSnake() {
  // Draw body (no blinking)
  for (int i = 1; i < snake_length; i++) {
    display.fillRect(snake_x[i], snake_y[i], SNAKE_SIZE, SNAKE_SIZE, SNAKE_BODY_COLOR);
  }

  // Draw head (based on blink state)
  if (isShow) {
    display.fillRect(snake_x[0], snake_y[0], SNAKE_SIZE, SNAKE_SIZE, SNAKE_HEAD_COLOR);
  } else {
    display.fillRect(snake_x[0], snake_y[0], SNAKE_SIZE, SNAKE_SIZE, SSD1306_BLACK);
  }
}

// -------------------------- Draw snake + food + head blink --------------------------
void blinkSnakeAndFood() {
  display.clearDisplay();
  drawScore();
  drawSnake();   // Draw snake (including blinking head)
  drawFood();    // Draw food
  display.display();
  isShow = !isShow;  // Toggle blink state
}