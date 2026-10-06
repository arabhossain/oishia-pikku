#include "snake.h"
#include <Arduino.h>

// -------------------------- Generate food not overlapping with snake --------------------------
void generateFood() {
  bool overlap;  // Whether overlaps with snake body
  do {
    overlap = false;
    // Food coordinates: within valid range, aligned with snake size (multiple of 4)
    food_x = (random(0, MAX_SNAKE_X / SNAKE_SIZE - 2)) * SNAKE_SIZE;
    food_y = (random(4, MAX_SNAKE_Y / SNAKE_SIZE )) * SNAKE_SIZE;

    // Check if overlaps with any snake segment
    for (int i = 0; i < snake_length; i++) {
      if (abs(snake_x[i] - food_x) < SNAKE_SIZE && abs(snake_y[i] - food_y) < SNAKE_SIZE) {
        overlap = true;
        break;
      }
    }
  } while (overlap);  // Regenerate if overlapping

  Serial.printf("Food generated: (%d,%d)\n", food_x, food_y);
}

// -------------------------- Draw food (circle) --------------------------
void drawFood() {
  int center_x = food_x + FOOD_SIZE / 2;
  int center_y = food_y + FOOD_SIZE / 2;
  display.fillCircle(center_x, center_y, FOOD_SIZE / 2, FOOD_COLOR);
}

// -------------------------- Check if food is eaten --------------------------
bool checkFoodCollision() {
  // Collision detection: snake head overlaps with food by more than half
  bool xOverlap = (snake_x[0] < food_x + FOOD_SIZE/2) && (snake_x[0] + SNAKE_SIZE > food_x + FOOD_SIZE/2);
  bool yOverlap = (snake_y[0] < food_y + FOOD_SIZE/2) && (snake_y[0] + SNAKE_SIZE > food_y + FOOD_SIZE/2);

  if (xOverlap && yOverlap) {
    // Increase snake length by 1 (not exceeding maximum)
    if (snake_length < MAX_LENGTH) {
      int tail_idx = snake_length - 1;  // Current tail index
      int new_tail_x = snake_x[tail_idx];
      int new_tail_y = snake_y[tail_idx];

      // New segment extends behind the tail (opposite direction)
      switch (current_dir) {
        case 0:  // Moving up → new segment below tail
          new_tail_y += SNAKE_SIZE;
          break;
        case 1:  // Moving right → new segment left of tail
          new_tail_x -= SNAKE_SIZE;
          break;
        case 2:  // Moving down → new segment above tail
          new_tail_y -= SNAKE_SIZE;
          break;
        case 3:  // Moving left → new segment right of tail
          new_tail_x += SNAKE_SIZE;
          break;
        default:
          new_tail_y += SNAKE_SIZE;  // Default below when no direction
          break;
      }

      // Assign new segment coordinates
      snake_x[snake_length] = new_tail_x;
      snake_y[snake_length] = new_tail_y;
      snake_length++;

      Serial.printf("Food eaten! Length: %d, new segment: (%d,%d)\n", snake_length, new_tail_x, new_tail_y);
    }
    return true;  // Food eaten, return true
  }
  return false;  // Not eaten
}