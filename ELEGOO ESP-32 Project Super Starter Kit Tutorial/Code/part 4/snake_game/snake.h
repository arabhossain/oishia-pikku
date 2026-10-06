#ifndef SNAKE_H
#define SNAKE_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -------------------------- Hardware Configuration Macros --------------------------
#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_ADDR   0x3C  // OLED I2C address (adjust as needed)



// -------------------------- Game Parameter Macros --------------------------
#define DEAD_ZONE   600   // Joystick dead zone
#define MOVE_STEP   1     // Movement step size
#define MOVE_SPEED  50    // Movement speed (ms)

#define SNAKE_SIZE  4     // Size of snake head/body/food (4x4 pixels)
#define FOOD_SIZE   4
#define MAX_LENGTH  30    // Maximum snake length
#define MAX_SNAKE_X (OLED_WIDTH - SNAKE_SIZE)  // Maximum valid X for snake head (124)
#define MAX_SNAKE_Y (OLED_HEIGHT - SNAKE_SIZE) // Maximum valid Y for snake head (60)

// -------------------------- Color Definitions --------------------------
#define FOOD_COLOR        SSD1306_WHITE
#define SNAKE_BODY_COLOR  SSD1306_WHITE
#define SNAKE_HEAD_COLOR  SSD1306_WHITE

// -------------------------- Global Object Declarations --------------------------
extern Adafruit_SSD1306 display;  // OLED display object

// -------------------------- Global Variable Declarations --------------------------
// Snake-related variables
extern int snake_x[MAX_LENGTH];    // X coordinates of each snake segment (0=head)
extern int snake_y[MAX_LENGTH];    // Y coordinates of each snake segment
extern int snake_length;           // Current snake length
extern bool isShow;                // Snake head blink state
extern int current_dir;            // Movement direction (-1=none, 0=up, 1=right, 2=down, 3=left)

// Food-related variables
extern int food_x;
extern int food_y;

// Game state variables
extern bool start_game;
extern bool isGameOver;

// -------------------------- Function Declarations --------------------------
// Snake core functions (Snake.cpp)
void resetSnake();                 // Reset snake to initial state
void moveSnake();                  // Snake movement (including body following)
bool checkSnakeOverBoundary();     // Check if snake head crosses boundary (returns true if out of bounds)

// Joystick functions (Joystick.cpp)
void initJoystick();               // Initialize joystick (pin modes)
void readJoystick();               // Read joystick direction and update current_dir
bool isJoystickButtonPressed();    // Check if joystick button is pressed

// Food functions (Food.cpp)
void generateFood();               // Generate food not overlapping with snake
void drawFood();                   // Draw food
bool checkFoodCollision();         // Check if food is eaten (returns true if eaten)

// Drawing functions (Snake.cpp)
void drawSnake();                  // Draw snake (head + body)
void blinkSnakeAndFood();          // Draw snake + food + head blink effect

#endif