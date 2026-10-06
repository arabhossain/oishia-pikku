#include "score.h"
#include <Arduino.h>
#include "snake.h"

// Global score variable initialization
int current_score = 0;

// -------------------------- Initialize score (reset to 0) --------------------------
void initScore() {
  current_score = 0;
  Serial.println("Score initialized: 0");
}

// -------------------------- Add score --------------------------
void addScore(int val) {
  current_score += val;
  Serial.printf("Score +%d, total: %d\n", val, current_score);
}

// -------------------------- Reset score (clear to 0) --------------------------
void resetScore() {
  current_score = 0;
  Serial.println("Score reset: 0");
}

// -------------------------- Draw score on OLED (top-left corner) --------------------------
void drawScore() {
  display.setTextSize(1);        // Font size (1=8x8 pixels)
  display.setTextColor(SSD1306_WHITE);  // White font
  display.setCursor(2, 2);       // Position (2,2) with margin from top-left
  display.print("Score: ");      // Text prefix
  display.print(current_score);  // Display current score
}