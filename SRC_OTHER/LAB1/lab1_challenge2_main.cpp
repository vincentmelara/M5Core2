#include <M5Core2.h>
#include <Wire.h>
#include "Adafruit_seesaw.h"

// LCD screen dimensions
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

// Variables
const int SDA_PIN = 32; // 21 for internal; 32 for port A
const int SCL_PIN = 33; // 22 for internal; 33 for port A

Adafruit_seesaw ss;

#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START    16
uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

// Dot positions
int blue_x = SCREEN_WIDTH / 2, blue_y = SCREEN_HEIGHT / 2;
int red_x = SCREEN_WIDTH / 4, red_y = SCREEN_HEIGHT / 4;

// Store last positions for optimized screen updates
int last_blue_x = blue_x, last_blue_y = blue_y;
int last_red_x = red_x, last_red_y = red_y;

const int DOT_SIZE = 4;  // Set to 1 for single-pixel dots
const int COLLISION_DISTANCE = 10;  // Distance for collision detection

// Speed variables
int blue_speed = 1;  // Speed of joystick-controlled blue dot
int red_speed = 1;   // Speed of D-pad-controlled red dot

// Timer variables
unsigned long start_time, end_time;
bool game_over = false;

// Function to update dots without clearing the whole screen
void updateDots() {
    if (game_over) return; // Stop updating if game is over

    // Erase old positions
    M5.Lcd.fillRect(last_blue_x, last_blue_y, DOT_SIZE, DOT_SIZE, BLACK);
    M5.Lcd.fillRect(last_red_x, last_red_y, DOT_SIZE, DOT_SIZE, BLACK);

    // Draw new positions
    M5.Lcd.fillRect(blue_x, blue_y, DOT_SIZE, DOT_SIZE, BLUE);
    M5.Lcd.fillRect(red_x, red_y, DOT_SIZE, DOT_SIZE, RED);

    // Update last known positions
    last_blue_x = blue_x;
    last_blue_y = blue_y;
    last_red_x = red_x;
    last_red_y = red_y;
}

// Function to check for collisions
void checkCollision() {
    int dx = blue_x - red_x;
    int dy = blue_y - red_y;
    int distance = sqrt(dx * dx + dy * dy); // Euclidean distance

    if (distance <= COLLISION_DISTANCE) {
        game_over = true;
        end_time = millis(); // Capture the end time

        float elapsed_time = (end_time - start_time) / 1000.0; // Convert to seconds

        // Display "Game Over" screen
        M5.Lcd.fillScreen(WHITE);
        M5.Lcd.setTextColor(RED);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setCursor(60, 100);
        M5.Lcd.print("GAME OVER!");

        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(70, 150);
        M5.Lcd.printf("Time: %.2fs", elapsed_time);
    }
}

void resetGame() {
    // Reset dot positions ensuring they don't collide
    blue_x = SCREEN_WIDTH / 3;  // Place blue dot at 1/3rd of the screen width
    blue_y = SCREEN_HEIGHT / 2; // Center vertically

    red_x = (2 * SCREEN_WIDTH) / 3; // Place red dot at 2/3rd of the screen width
    red_y = SCREEN_HEIGHT / 2; // Center vertically

    // Reset last positions
    last_blue_x = blue_x;
    last_blue_y = blue_y;
    last_red_x = red_x;
    last_red_y = red_y;

    // Reset speed settings
    blue_speed = 1;
    red_speed = 1;

    // Reset timer
    start_time = millis();
    game_over = false;

    // Clear screen and redraw dots
    M5.Lcd.fillScreen(BLACK);
    updateDots();
}


// Setup function
void setup() {
    M5.begin();
    Wire.begin(SDA_PIN, SCL_PIN, 400000); // Initialize I2C port with 400kHz speed

    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("Gamepad QT example!");

    if (!ss.begin(0x50)) {
        Serial.println("ERROR! seesaw not found");
        while (1) delay(1);
    }
    Serial.println("seesaw started");

    uint32_t version = ((ss.getVersion() >> 16) & 0xFFFF);
    if (version != 5743) {
        Serial.print("Wrong firmware loaded? ");
        Serial.println(version);
        while (1) delay(10);
    }
    Serial.println("Found Product 5743");

    ss.pinModeBulk(button_mask, INPUT_PULLUP);
    ss.setGPIOInterrupts(button_mask, 1);

    // Start game timer
    start_time = millis();

    // Clear screen and draw initial dots
    M5.Lcd.fillScreen(BLACK);
    updateDots();
}

// Loop function
void loop() {
    M5.update(); // Ensure button press detection is fresh

     // Check if M5 Button A is pressed for reset
     if (M5.BtnA.wasPressed()) {
        resetGame();
        return;  // Avoid continuing the loop after reset
    }


    if (game_over) return; // Stop updating if game is over

    delay(20); // Small delay to manage update speed

    // Read joystick values
    int x = 1023 - ss.analogRead(14);
    int y = 1023 - ss.analogRead(15);

    // Move blue dot with joystick
    if (x > 600) blue_x = min(SCREEN_WIDTH - DOT_SIZE, blue_x + blue_speed); // Move Right
    if (x < 400) blue_x = max(0, blue_x - blue_speed); // Move Left
    if (y > 600) blue_y = max(0, blue_y - blue_speed); // ↑ Change this to reverse up/down
    if (y < 400) blue_y = min(SCREEN_HEIGHT - DOT_SIZE, blue_y + blue_speed); // ↓ Change this to reverse up/down

    // Read button states
    uint32_t buttons = ss.digitalReadBulk(button_mask);

    // Move red dot with D-pad buttons
    if (!(buttons & (1UL << BUTTON_B))) red_y = min(SCREEN_HEIGHT - DOT_SIZE, red_y + red_speed); // Move Down
    if (!(buttons & (1UL << BUTTON_Y))) red_x = max(0, red_x - red_speed); // Move Left
    if (!(buttons & (1UL << BUTTON_X))) red_y = max(0, red_y - red_speed); // Move Up
    if (!(buttons & (1UL << BUTTON_A))) red_x = min(SCREEN_WIDTH - DOT_SIZE, red_x + red_speed); // Move Right

    // Increase blue dot speed with Start button (loop from 1 to 5)
    if (!(buttons & (1UL << BUTTON_START))) {
        blue_speed = (blue_speed % 5) + 1;
        Serial.printf("Blue speed: %d\n", blue_speed);
        delay(200); // Prevent multiple fast increments
    }

    // Increase red dot speed with Select button (loop from 1 to 5)
    if (!(buttons & (1UL << BUTTON_SELECT))) {
        red_speed = (red_speed % 5) + 1;
        Serial.printf("Red speed: %d\n", red_speed);
        delay(200); // Prevent multiple fast increments
    }

    // Update dots on the screen
    updateDots();

    // Check for collision
    checkCollision();
}
