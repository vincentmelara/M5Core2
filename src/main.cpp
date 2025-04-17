#include <M5Core2.h>
#include <Wire.h>
#include "Adafruit_seesaw.h"
#include "ImageArrays.h"  // Include your image data
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>

int startX = 0;
int startY = 0;
int screenWidth = 320;
int screenHeight = 240;

// Variables
const int SDA_PIN = 32; // 21 for internal; 32 for port A
const int SCL_PIN = 33; // 22 for internal; 33 for port A

Adafruit_seesaw ss;

#define JOY_X 14
#define JOY_Y 15

int joyThresholdLow = 400;   // Adjust if needed
int joyThresholdHigh = 600;
unsigned long lastMoveTime = 0;
int moveDelay = 80;         // Delay between moves in ms

#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START    16
uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

struct ShipButton {
const uint16_t* image;
int width;
int height;
int x;
int y;
};

struct Ship {
    const char* name;
    const uint16_t* image;
    int width, height;
    int x, y;
    int length;
    int row;
    int col;
    bool placed;
    bool isHorizontal;

    // ✅ Constructor to match your initialization
    Ship(const char* n, const uint16_t* img, int w, int h, int px, int py, int len,
         int r = 0, int c = 0, bool p = false, bool horiz = true)
        : name(n), image(img), width(w), height(h), x(px), y(py),
          length(len), row(r), col(c), placed(p), isHorizontal(horiz) {}
};

const int NUM_SHIPS = 6;
Ship ships[NUM_SHIPS] = {
    //name         image        wh   ht  x     y    len  row  col placed  isHorizontal
    {"Submarine",  submarine,   30,  30, 222,  22,  2,   0,   0,  false,  true},
    {"Frigate",    frigate,     42,  16, 222,  67,  3,   0,   0,  false,  true},
    {"Destroyer",  destroyer,   65,  21, 222,  92,  3,   0,   0,  false,  true},
    {"Cruiser",    cruiser,     80,  19, 222,  117, 4,   0,   0,  false,  true},
    {"Battleship", battleship, 100,  19, 217,  142, 5,   0,   0,  false,  true},
    {"Carrier",    carrier,    100,  49, 217,  167, 6,   0,   0,  false,  true}
};

int selectedShipIndex = 0;  // Start with submarine selected
int prevSelectedShipIndex = 0;

// Submarine board position (row and col on the 10x10 grid)
int subRow = 0;
int subCol = 0;

// To track current screen mode
bool inPlacementMode = false;
bool gameStarted = false;


// Submarine size
const int subLength = 2;
bool subIsHorizontal = true;

// Grid configuration
int gridX = 22;       // Starting X position of the grid
int gridY = 22;       // Starting Y position of the grid
int cellSize = 19;    // Width and height of each grid cell
int gridSize = 10;    // 10x10 grid


    // === Draw the ship image to the right of the grid ===

// Place image 20 pixels to the right of the grid
int imgX = gridX + gridSize * cellSize + 20;
int imgY = gridY;  // Align it with the top of the grid, or adjust if you want to center vertically

const int maxPlacedCoords = 100;  // Change this depending on how many total cells max
int placedCoords[maxPlacedCoords][2];
int placedCount = 0;

void drawImage(const uint16_t* bitmap, int imgWidth, int imgHeight, int startX, int startY);
void drawGameScreen();
void drawShips();
void drawShipOnBoard(Ship& ship);
void redrawShipSelection(int oldIndex, int newIndex);
void printPlacedCoords();
bool doesOverlap(const Ship& ship);
void drawAllPlacedShips();
bool allShipsPlaced();
void drawStartGameButton();


BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

#define SERVICE_UUID        "9f9a1891-ab9d-4e3c-8f61-b790f71ceeb1"
#define CHARACTERISTIC_UUID "68a920cb-bd3f-40a9-8571-208497b9926d"


//-------SETUP-------------
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

    drawImage(splash, 320, 240, 0, 0);

}

void setupBLEServer() {
    Serial.println("🔵 Initializing BLE server...");
    BLEDevice::init("BattleshipHost");

    pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_NOTIFY |
                        BLECharacteristic::PROPERTY_INDICATE
                      );

    // Enable notification support
    pCharacteristic->addDescriptor(new BLE2902());

    // Start the service and begin advertising
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->start();
    Serial.println("✅ BLE advertising started. Waiting for connection...");
}


void loop() {
    M5.update();

    //get selected ship
    Ship& currentShip = ships[selectedShipIndex];

    uint32_t buttons = ss.digitalReadBulk(button_mask);

    if (allShipsPlaced() && (buttons & (1UL << BUTTON_START)) == 0) {
        Serial.println("🚀 Start Game Pressed!");

        // Clear screen and show waiting message
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(40, 100);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.print("Waiting for player 2...");
    
        // Start BLE server
        setupBLEServer();
    
        delay(3000); // Give some time for a device to connect
    
        // Convert placed coordinates to a single string
        String coordString = "";
        for (int i = 0; i < placedCount; i++) {
            coordString += String(placedCoords[i][0]) + "," + String(placedCoords[i][1]);
            if (i < placedCount - 1) coordString += ";";
        }
    
        // Send the data via BLE notification
        pCharacteristic->setValue(coordString.c_str());
        pCharacteristic->notify();
    
        Serial.println("📡 Coordinates sent via BLE:");
        Serial.println(coordString);
        M5.Lcd.setCursor(40, 140);
        M5.Lcd.print("Coords sent!");
    
    }

    static bool xPrev = false;
    static bool bPrev = false;
    static bool selectPrev = false;
    static bool yPrev = false;

    bool xPressed = !(buttons & (1UL << BUTTON_X));
    bool bPressed = !(buttons & (1UL << BUTTON_B));
    bool selectPressed = !(buttons & (1UL << BUTTON_SELECT));
    bool yPressed = !(buttons & (1UL << BUTTON_Y));

    // START enters game screen + ship selection
    if (!gameStarted && !(buttons & (1UL << BUTTON_START))) {
        M5.Lcd.fillScreen(BLACK);
        drawGameScreen();
        drawShips();
        drawStartGameButton();
        inPlacementMode = false;
        gameStarted = true;

        // Manually draw selection (so the Submarine name appears)
        redrawShipSelection(selectedShipIndex, selectedShipIndex);

        // Reset previous button states to prevent immediate accidental navigation
        xPrev = true;
        bPrev = true;
        selectPrev = true;
        yPrev = true;

        delay(300); // small delay to let button release
    }


    // If game hasn't started, don't do anything else
    if (!gameStarted) return;

    // SELECT enters placement mode for any unplaced ship
    if (selectPressed && !selectPrev) {
        if (!inPlacementMode && !currentShip.placed) {
            // 🔵 Enter placement mode
            inPlacementMode = true;
            drawGameScreen();
            drawShips();
            drawAllPlacedShips();
            drawShipOnBoard(currentShip);
            lastMoveTime = millis();
        } else if (inPlacementMode) {
            // 🔴 Exit placement mode (cancel placement)
            inPlacementMode = false;
            drawGameScreen();
            drawShips();
            drawAllPlacedShips();
            redrawShipSelection(selectedShipIndex, selectedShipIndex); // Re-show red box + name
        }
    }


    // Placement mode - move submarine with joystick
    if (inPlacementMode) {
        static unsigned long lastXPress = 0;
        int joyX = ss.analogRead(JOY_X);
        int joyY = ss.analogRead(JOY_Y);

        unsigned long now = millis();
        if (now - lastMoveTime > moveDelay) {
            if (joyY < joyThresholdLow && currentShip.row > 0) {
                currentShip.row--;
                lastMoveTime = now;
            } else if (joyY > joyThresholdHigh && currentShip.row < gridSize - 1) {
                currentShip.row++;
                lastMoveTime = now;
            }
            
            if (joyX > joyThresholdHigh && currentShip.col > 0) {
                currentShip.col--;
                lastMoveTime = now;
            } else if (joyX < joyThresholdLow && currentShip.col < gridSize - currentShip.length) {
                currentShip.col++;
                lastMoveTime = now;
            }
            

            //draw given ship at new position
            drawShipOnBoard(currentShip);
            drawAllPlacedShips();
        }

        // ✅ Y button confirms placement
        if (yPressed && !yPrev) {
            // 1. Check grid bounds (horizontal placement only)
            if (currentShip.col + currentShip.length > gridSize || currentShip.row >= gridSize) {
                Serial.println("❌ Placement out of bounds!");
                // Flash red
                for (int i = 0; i < currentShip.length; i++) {
                    int x = gridX + (currentShip.col + i) * cellSize;
                    int y = gridY + currentShip.row * cellSize;
                    M5.Lcd.fillRect(x + 1, y + 1, cellSize - 2, cellSize - 2, RED);
                }
                delay(300);
                drawShipOnBoard(currentShip);
                return;
            }

            // 2. Check overlap
            if (doesOverlap(currentShip)) {
                Serial.println("⚠️ Overlap detected! Choose another position.");
                for (int i = 0; i < currentShip.length; i++) {
                    int x = gridX + (currentShip.col + i) * cellSize;
                    int y = gridY + currentShip.row * cellSize;
                    M5.Lcd.fillRect(x + 1, y + 1, cellSize - 2, cellSize - 2, RED);
                }
                delay(300);
                drawShipOnBoard(currentShip);
                return;
            }

            // 3. If everything is valid, place the ship
            for (int i = 0; i < currentShip.length; i++) {
                int row = currentShip.row;
                int col = currentShip.col + i;
                if (placedCount < maxPlacedCoords) {
                    placedCoords[placedCount][0] = row;
                    placedCoords[placedCount][1] = col;
                    placedCount++;
                } else {
                    Serial.println("🚫 Too many coordinates! Max reached.");
                    break;
                }
            }

            currentShip.placed = true;
            yPressed = false;
            inPlacementMode = false;
            Serial.print("inPlacementMode: ");
            Serial.println(inPlacementMode);
            Serial.print("Ship marked placed: ");
            Serial.println(currentShip.placed);


            Serial.print("✅ ");
            Serial.print(currentShip.name);
            Serial.println(" placed successfully!");

            drawShips();
            drawAllPlacedShips();
            printPlacedCoords();
            if (allShipsPlaced()) {
                drawStartGameButton();
            }            
        }

    } else {
        // Navigate ship list (X/B)
        if (xPressed && !xPrev) {
            prevSelectedShipIndex = selectedShipIndex;
            selectedShipIndex = (selectedShipIndex - 1 + NUM_SHIPS) % NUM_SHIPS;
            redrawShipSelection(prevSelectedShipIndex, selectedShipIndex);
        }
        if (bPressed && !bPrev) {
            prevSelectedShipIndex = selectedShipIndex;
            selectedShipIndex = (selectedShipIndex + 1) % NUM_SHIPS;
            redrawShipSelection(prevSelectedShipIndex, selectedShipIndex);
        }
    }

    // Update previous button states
    xPrev = xPressed;
    bPrev = bPressed;
    selectPrev = selectPressed;
    yPrev = yPressed;
}




void drawImage(const uint16_t* bitmap, int imgWidth, int imgHeight, int startX, int startY) {
    int resizeMult = 1;
    // Draw each pixel with optional scaling
    for (int y = 0; y < imgHeight; y++) {
        for (int x = 0; x < imgWidth; x++) {
            int pixNum = (y * imgWidth) + x;
            uint16_t pixel = bitmap[pixNum];

            if (pixel != 0) { // Treat 0 as transparent
                byte red   = (pixel >> 11) & 0x1F; red   <<= 3;
                byte green = (pixel >> 5)  & 0x3F; green <<= 2;
                byte blue  = pixel         & 0x1F; blue  <<= 3;

                for (int i = 0; i < resizeMult; i++) {
                    for (int j = 0; j < resizeMult; j++) {
                        int xDraw = startX + x * resizeMult + i;
                        int yDraw = startY + y * resizeMult + j;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}

void drawGameScreen(){
    M5.Lcd.setTextSize(1);


  // Draw grid lines
  for (int i = 0; i <= gridSize; i++) {
    // Vertical lines
    M5.Lcd.drawLine(gridX + i * cellSize, gridY,
                    gridX + i * cellSize, gridY + gridSize * cellSize, WHITE);
    // Horizontal lines
    M5.Lcd.drawLine(gridX, gridY + i * cellSize,
                    gridX + gridSize * cellSize, gridY + i * cellSize, WHITE);
  }

  // Draw X-axis number boxes and numbers (1–10)
  for (int i = 0; i < gridSize; i++) {
    int x = gridX + i * cellSize + 1;
    int y = gridY - cellSize;

    // Draw box
    M5.Lcd.fillRect(x, y, cellSize, cellSize, WHITE);

    // Draw number
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(BLUE);
    if (i == 9) {
        M5.Lcd.setCursor(x - 1, y + 4);  // Shift "10" left
        
        // Manually draw '1' and '0' with tighter spacing
        M5.Lcd.print("1");
        M5.Lcd.setCursor(x + 8, y + 4); // Position for '0' closer to '1'
        M5.Lcd.print("0");
      } else {
        M5.Lcd.setCursor(x + 4, y + 4);
        M5.Lcd.printf("%d", i + 1);
      }
  }

  // Draw Y-axis letter boxes and letters (A–J)
  for (int i = 0; i < gridSize; i++) {
    int x = gridX - cellSize;
    int y = gridY + i * cellSize + 1;

    // Draw box
    M5.Lcd.fillRect(x, y, cellSize, cellSize, WHITE);

    // Draw letter
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(BLUE);
    M5.Lcd.setCursor(x + 4, y + 2);
    M5.Lcd.printf("%c", 'A' + i);
  }
}


void drawShips() {
    // shipButtons[0] = {submarine, 30, 30, imgX, imgY};
    // shipButtons[1] = {frigate, 42, 16, imgX, imgY + 45};
    // shipButtons[2] = {destroyer, 65, 21, imgX, imgY + 70};
    // shipButtons[3] = {cruiser, 80, 19, imgX - 10, imgY + 95};
    // shipButtons[4] = {battleship, 100, 19, imgX - 15, imgY + 120};
    // shipButtons[5] = {carrier, 100, 49, imgX - 15, imgY + 145};
  
    for (int i = 0; i < NUM_SHIPS; i++) {
        Ship& ship = ships[i];
  
        M5.Lcd.fillRect(ship.x - 2, ship.y - 2, ship.width + 4, ship.height + 4, BLACK);
        drawImage(ship.image, ship.width, ship.height, ship.x, ship.y);
        
        if (!inPlacementMode && i == selectedShipIndex) {
            M5.Lcd.drawRect(ship.x - 2, ship.y - 2, ship.width + 4, ship.height + 4, RED);
        }        
    }
  }
  

int prevSubRow = 0;
int prevSubCol = 0;

void drawShipOnBoard(Ship& ship) {
    static int prevRow = 0, prevCol = 0;
  
    // Clear previous position
    for (int i = 0; i < ship.length; i++) {
      int px = gridX + (prevCol + i) * cellSize;
      int py = gridY + prevRow * cellSize;
      M5.Lcd.fillRect(px + 1, py + 1, cellSize - 2, cellSize - 2, BLACK);
    }
  
    // Draw current position
    for (int i = 0; i < ship.length; i++) {
      int x = gridX + (ship.col + i) * cellSize;
      int y = gridY + ship.row * cellSize;
      M5.Lcd.fillRect(x + 1, y + 1, cellSize - 2, cellSize - 2, BLUE);
    }
  
    prevRow = ship.row;
    prevCol = ship.col;
}

void drawAllPlacedShips() {
    for (int i = 0; i < NUM_SHIPS; i++) {
      Ship& ship = ships[i];
      if (ship.placed) {
        for (int j = 0; j < ship.length; j++) {
          int x = gridX + (ship.col + j) * cellSize;
          int y = gridY + ship.row * cellSize;
          M5.Lcd.fillRect(x + 1, y + 1, cellSize - 2, cellSize - 2, BLUE);
        }
      }
    }
  }
  

  void redrawShipSelection(int oldIndex, int newIndex) {
    if (oldIndex != newIndex) {
        Ship& oldShip = ships[oldIndex];
        M5.Lcd.fillRect(oldShip.x - 2, oldShip.y - 2, oldShip.width + 4, oldShip.height + 4, BLACK);
        drawImage(oldShip.image, oldShip.width, oldShip.height, oldShip.x, oldShip.y);
    }

    Ship& newShip = ships[newIndex];
    M5.Lcd.fillRect(newShip.x - 2, newShip.y - 2, newShip.width + 4, newShip.height + 4, BLACK);
    drawImage(newShip.image, newShip.width, newShip.height, newShip.x, newShip.y);

    if (!inPlacementMode) {
        M5.Lcd.drawRect(newShip.x - 2, newShip.y - 2, newShip.width + 4, newShip.height + 4, RED);
    }

    if(!allShipsPlaced()){
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(GREEN, BLACK); // Green text with black background
        M5.Lcd.fillRect(110, 220, 120, 20, BLACK); // Clear previous name area
        M5.Lcd.setCursor(110, 220);
        M5.Lcd.print(newShip.name);
    }
}


bool doesOverlap(const Ship& ship) {
    for (int i = 0; i < ship.length; i++) {
      int row = ship.row;
      int col = ship.col + i;
  
      for (int j = 0; j < placedCount; j++) {
        if (placedCoords[j][0] == row && placedCoords[j][1] == col) {
          return true;  // Conflict found
        }
      }
    }
    return false;  // No conflicts
  }


  void printPlacedCoords() {
    Serial.println("====== All Placed Ships ======");
    for (int i = 0; i < NUM_SHIPS; i++) {
        Ship& ship = ships[i];
        if (ship.placed) {
            Serial.print(ship.name);
            Serial.println(" placed at:");

            for (int j = 0; j < ship.length; j++) {
                int row = ship.row + (ship.isHorizontal ? 0 : j);
                int col = ship.col + (ship.isHorizontal ? j : 0);
                Serial.print("  • Row ");
                Serial.print(row);
                Serial.print(", Col ");
                Serial.println(col);
            }

            Serial.println();
        }
    }
    Serial.println("==============================");
}

void exportPlacedCoords() {
    Serial.print("Sending placedCoords (");
    Serial.print(placedCount);
    Serial.println(" cells):");

    for (int i = 0; i < placedCount; i++) {
        Serial.print(placedCoords[i][0]); // row
        Serial.print(",");
        Serial.print(placedCoords[i][1]); // col

        if (i < placedCount - 1) Serial.print(";"); // delimiter
    }

    Serial.println(); // newline after the full message
}

bool allShipsPlaced() {
    for (int i = 0; i < NUM_SHIPS; i++) {
        if (!ships[i].placed) return false;
    }
    return true;
}

void drawStartGameButton() {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(BLACK); // Green text with black background
    M5.Lcd.setCursor(110, 220);
    M5.Lcd.fillRect(110, 220, 120, 20, BLACK); // Clear previous name area
    M5.Lcd.fillRect(110, 220, 120, 20, GREEN); // Clear previous name area
    M5.Lcd.print("Start Game");
}