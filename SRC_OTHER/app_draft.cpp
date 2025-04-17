#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Speaker.h>
#include "EGR425_Phase1_weather_bitmap_images.h"
#include "WiFi.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "./I2C_RW.h"

// Variables
// TODO 3: Register for openweather account and get API key
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "234a0b7be99d32d485d764d1c7e3246e";

// TODO 1: WiFi variables
String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "L@ncerN@tion";

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelay = 4000;  // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)

// LCD variables
int sWidth;
int sHeight;


// Weather/zip variables
String strWeatherIcon = "";
String strWeatherDesc;
String cityName;
double tempNow;
double tempMin;
double tempMax;
int utcOffsetInSeconds = -8 * 3600;
bool isCelsius = false;  // Toggle to switch between F and C

// Create TouchButtons for each digit
Button digitUpButtons[5] = {
    Button(50, 50, 40, 40, false, "Up1"),
    Button(90, 50, 40, 40, false, "Up2"),
    Button(130, 50, 40, 40, false, "Up3"),
    Button(170, 50, 40, 40, false, "Up4"),
    Button(210, 50, 40, 40, false, "Up5"),
};
Button digitDownButtons[5] = {
    Button(50, 150, 40, 40, false, "Down1"),
    Button(90, 150, 40, 40, false, "Down2"),
    Button(130, 150, 40, 40, false, "Down3"),
    Button(170, 150, 40, 40, false, "Down4"),
    Button(210, 150, 40, 40, false, "Down5"),
};

// "Save" button
Button saveButton(100, 200, 120, 40, false, "Save");

String zipCode = "92504";  // Default zipcode
int selectedDigit = 0;  // Which zipcode digit is currently selected (0-4)
String currentScreen = "weather";  // default to start on the weather screen

//initialize ntpClient
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


// Method header declarations
String httpGETRequest(const char* serverName);
void drawWeatherImage(String iconId, int resizeMult);
void fetchWeatherDetails();
void drawWeatherDisplay();
String getFormatted12HourTime();  // Function prototype

// Put your setup code here, to run once
void setup() {

    // Initialize the device
    M5.begin();
    M5.Lcd.setTextSize(2);
    
    // Set screen orientation and get height/width 
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    // TODO 2: Connect to WiFi
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());

    //initialize VCNL4040 device 
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);
    I2C_RW::initI2C(SHT_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);
    I2C_RW::scanI2cLinesForAddresses(false);

    // Write registers to initialize/enable VCNL sensors
    I2C_RW::initVCNL4040Sensors();

    timeClient.begin();
    timeClient.setTimeOffset(utcOffsetInSeconds);  // <-- Set the offset for local time

    drawWeatherDisplay();  // Start with weather screen
}

// This method fetches the weather details from the OpenWeather
// API and saves them into the fields defined above
void fetchWeatherDetails() {
    // Hardcode the specific city,state,country into the query
    // Examples: https://api.openweathermap.org/data/2.5/weather?q=riverside,ca,usa&units=imperial&appid=YOUR_API_KEY
    String serverURL = urlOpenWeather + "zip=" + zipCode + ",us&units=imperial&appid=" + apiKey;
    // Serial.println(serverURL); // Debug print

    // Make GET request and store reponse
    String response = httpGETRequest(serverURL.c_str());
    // Serial.print(response); // Debug print
    
    // Import ArduinoJSON Library and then use arduinojson.org/v6/assistant to
    // compute the proper capacity (this is a weird library thing) and initialize
    // the json object
    const size_t jsonCapacity = 768+250;
    DynamicJsonDocument objResponse(jsonCapacity);

    // Deserialize the JSON document and test if parsing succeeded
    DeserializationError error = deserializeJson(objResponse, response);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    //serializeJsonPretty(objResponse, Serial); // Debug print

    // Parse Response to get the weather description and icon
    JsonArray arrWeather = objResponse["weather"];
    JsonObject objWeather0 = arrWeather[0];
    String desc = objWeather0["main"];
    String icon = objWeather0["icon"];
    String city = objResponse["name"];

    // ArduionJson library will not let us save directly to these
    // variables in the 3 lines above for unknown reason
    strWeatherDesc = desc;
    strWeatherIcon = icon;
    cityName = city;

    // Parse response to get the temperatures
    JsonObject objMain = objResponse["main"];
    tempNow = objMain["temp"];
    tempMin = objMain["temp_min"];
    tempMax = objMain["temp_max"];
    // Serial.printf("NOW: %.1f F and %s\tMIN: %.1f F\tMax: %.1f F\n", tempNow, strWeatherDesc, tempMin, tempMax);
}


String getFormatted12HourTime() {
    time_t rawTime = timeClient.getEpochTime();  // Get time in seconds
    struct tm * timeInfo = localtime(&rawTime);

    int hour = timeInfo->tm_hour;
    int minute = timeInfo->tm_min;
    int second = timeInfo->tm_sec;

    String ampm = "AM";
    if (hour >= 12) {
        ampm = "PM";
        if (hour > 12) hour -= 12;  // Convert to 12-hour format
    }
    if (hour == 0) hour = 12;  // Midnight edge case

    char buffer[10];
    sprintf(buffer, "%02d:%02d:%02d %s", hour, minute, second, ampm.c_str());
    return String(buffer);
}


// Update the display based on the weather variables defined at the top of the screen.
void drawWeatherDisplay() {
    static String lastCity = "";
    static double lastTempNow = -999;
    static bool lastIsCelsius = false;

    // Only clear the screen if a major update occurs
    if (lastCity != cityName || lastIsCelsius != isCelsius) {
        lastCity = cityName;
        lastIsCelsius = isCelsius;

        // Background - light blue if daytime, navy blue if night
        if (strWeatherIcon.indexOf("d") >= 0) {
            M5.Lcd.fillScreen(TFT_CYAN);
        } else {
            M5.Lcd.fillScreen(TFT_NAVY);
        }

        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setTextSize(3);
        
        // Determine city name width
        int textWidth = cityName.length() * 18; // Approximate width per character at size 3
        int maxWidth = M5.Lcd.width() - 20; // Allow some padding

        // If the city name fits, print normally
        int textX = 10, textY = 10;
        if (textWidth <= maxWidth) {
            M5.Lcd.setCursor(textX, textY);
            M5.Lcd.printf("City: %s", cityName.c_str());
        } 
        // Otherwise, wrap to the next line
        else {
            int splitIndex = cityName.length() / 2; // Split approximately in half
            String firstLine = cityName.substring(0, splitIndex);
            String secondLine = cityName.substring(splitIndex);

            M5.Lcd.setCursor(textX, textY);
            M5.Lcd.printf("City: %s", firstLine.c_str());
            
            M5.Lcd.setCursor(textX, textY + 30); // Move down for second line
            M5.Lcd.printf("%s", secondLine.c_str());
        }
    }

    // Convert temperatures
    double displayTempNow = isCelsius ? (tempNow - 32) * 5.0 / 9.0 : tempNow;
    
    // Update temperature only if it changed
    if (lastTempNow != displayTempNow) {
        lastTempNow = displayTempNow;
        M5.Lcd.setCursor(10, 60);
        M5.Lcd.setTextSize(4);
        M5.Lcd.printf("Temp: %.1f %s", displayTempNow, isCelsius ? "C" : "F");
    }

    // Update timestamp
    String syncTime = getFormatted12HourTime();
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Last Sync: %s", syncTime.c_str());
}


void drawZipCodeScreen() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(50, 20);
    M5.Lcd.print("Edit Zip Code");

    // Define layout positions
    int startX = 30;  // Start further left for centering
    int buttonWidth = 40;
    int buttonHeight = 40;
    int spacing = 5;
    int totalWidth = (buttonWidth * 5) + (spacing * 4);
    int centerX = (M5.Lcd.width() - totalWidth) / 2;

    // Draw each digit and buttons
    for (int i = 0; i < 5; i++) {
        int xPos = centerX + i * (buttonWidth + spacing);
        int digit = zipCode[i] - '0';

        // Draw the current digit (Centered between up/down buttons)
        M5.Lcd.setCursor(xPos + 10, 100);
        M5.Lcd.setTextSize(4);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.printf("%d", digit);

        // Define button colors
        uint16_t arrowFillColor = TFT_WHITE;
        uint16_t arrowBorderColor = TFT_DARKGREY;
        uint16_t shadowColor = TFT_BLACK;

        // Draw up button (above the digit) - 8-bit style
        M5.Lcd.fillTriangle(xPos, 72, xPos + 20, 48, xPos + 40, 72, shadowColor);  // Shadow
        M5.Lcd.fillTriangle(xPos + 1, 70, xPos + 20, 50, xPos + 39, 70, arrowBorderColor);  // Border
        M5.Lcd.fillTriangle(xPos + 3, 68, xPos + 20, 52, xPos + 37, 68, arrowFillColor);  // Inner arrow
        
        // Draw down button (below the digit) - 8-bit style
        M5.Lcd.fillTriangle(xPos, 158, xPos + 20, 182, xPos + 40, 158, shadowColor);  // Shadow
        M5.Lcd.fillTriangle(xPos + 1, 160, xPos + 20, 180, xPos + 39, 160, arrowBorderColor);  // Border
        M5.Lcd.fillTriangle(xPos + 3, 162, xPos + 20, 178, xPos + 37, 162, arrowFillColor);  // Inner arrow
    }

    // Draw "Save" button with an 8-bit style and rounded corners
int saveButtonWidth = 120;
int saveButtonHeight = 40;
int saveButtonX = (M5.Lcd.width() - saveButtonWidth) / 2;
int saveButtonY = 190;
int cornerRadius = 8;  // Adjust corner radius for a retro look

// Outer border for 8-bit effect (Darker Blue)
M5.Lcd.fillRoundRect(saveButtonX - 2, saveButtonY - 2, saveButtonWidth + 4, saveButtonHeight + 4, cornerRadius, TFT_BLUE);

// Inner button background (Main Blue)
M5.Lcd.fillRoundRect(saveButtonX, saveButtonY, saveButtonWidth, saveButtonHeight, cornerRadius, TFT_BLUE);

// "Pixel Shadow" Effect - Draw a smaller black rectangle at the bottom to simulate depth
M5.Lcd.fillRect(saveButtonX + 3, saveButtonY + saveButtonHeight - 8, saveButtonWidth - 6, 6, TFT_BLACK);

// Draw "Save" text with an 8-bit pixelated look
M5.Lcd.setCursor(saveButtonX + 20, saveButtonY + 10);
M5.Lcd.setTextSize(2);
M5.Lcd.setTextColor(TFT_WHITE);
M5.Lcd.print("SAVE");

// Optional: Add "shiny" pixel effect at the top-left for extra 8-bit style
M5.Lcd.fillRect(saveButtonX + 4, saveButtonY + 4, 10, 4, TFT_CYAN);
M5.Lcd.fillRect(saveButtonX + 4, saveButtonY + 8, 6, 2, TFT_CYAN);
}

void drawTempAndHumidity() {
    M5.Lcd.fillScreen(TFT_GREEN);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(3);
}



// This method takes in a URL and makes a GET request to the
// URL, returning the response.
String httpGETRequest(const char* serverURL) {
    
    // Initialize client
    HTTPClient http;
    http.begin(serverURL);

    // Send HTTP GET request and obtain response
    int httpResponseCode = http.GET();
    String response = http.getString();

    // // Check if got an error
    // if (httpResponseCode > 0) 
    //     Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    // else {
    //     // Serial.printf("HTTP Response ERROR code: %d\n", httpResponseCode);
    //     // Serial.printf("Server Response: %s\n", response);
    // }

    // Free resources and return response
    http.end();
    return response;
}

// This method takes in an image icon string (from API) and a 
// resize multiple and draws the corresponding image (bitmap byte
// arrays found in EGR425_Phase1_weather_bitmap_images.h) to scale (for 
// example, if resizeMult==2, will draw the image as 200x200 instead
// of the native 100x100 pixels) on the right-hand side of the
// screen (centered vertically). 
void drawWeatherImage(String iconId, int resizeMult) {

    // Get the corresponding byte array
    const uint16_t * weatherBitmap = getWeatherBitmap(iconId);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    int yOffset = -(resizeMult * imgSqDim - M5.Lcd.height()) / 2;
    int xOffset = sWidth - (imgSqDim*resizeMult*.8); // Right align (image doesn't take up entire array)
    //int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally
    
    // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
    for (int y = 0; y < imgSqDim; y++) {
        for (int x = 0; x < imgSqDim; x++) {
            // Compute the linear index in the array and get pixel value
            int pixNum = (y * imgSqDim) + x;
            uint16_t pixel = weatherBitmap[pixNum];

            // If the pixel is black, do NOT draw (treat it as transparent);
            // otherwise, draw the value
            if (pixel != 0) {
                // 16-bit RBG565 values give the high 5 pixels to red, the middle
                // 6 pixels to green and the low 5 pixels to blue as described
                // here: http://www.barth-dev.de/online/rgb565-color-picker/
                byte red = (pixel >> 11) & 0b0000000000011111;
                red = red << 3;
                byte green = (pixel >> 5) & 0b0000000000111111;
                green = green << 2;
                byte blue = pixel & 0b0000000000011111;
                blue = blue << 3;

                // Scale image; for example, if resizeMult == 2, draw a 2x2
                // filled square for each original pixel
                for (int i = 0; i < resizeMult; i++) {
                    for (int j = 0; j < resizeMult; j++) {
                        int xDraw = x * resizeMult + i + xOffset;
                        int yDraw = y * resizeMult + j + yOffset;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}


// Put your main code here, to run repeatedly
void loop() {
    M5.update();  // Update touch events

    static String lastScreen = "";  // Track the last screen to prevent unnecessary redraws

    if (currentScreen == "weather") {
        // Force redraw when switching screens
        if (lastScreen != "weather") {
            drawWeatherDisplay();
            lastScreen = "weather";
        }

        // Toggle between Celsius and Fahrenheit
        if (M5.BtnA.wasPressed()) {
            Serial.println("Button A pressed - Toggling Temperature Unit");
            isCelsius = !isCelsius;
            // currentScreen = "weather";
            // lastScreen = "";  // Force redraw when switching back
            drawWeatherDisplay();  // 🔹 IMMEDIATELY redraw the weather display
        }

        // Update weather only when timer expires
        if ((millis() - lastTime) > timerDelay) {
            if (WiFi.status() == WL_CONNECTED) {
                timeClient.update();
                fetchWeatherDetails();
                drawWeatherDisplay();
                I2C_RW::displayVCNL4040Data();
                I2C_RW::displaySHT40Data();
            } else {
                Serial.println("WiFi Disconnected");
            }
            lastTime = millis();
        }

        // Go to zip code edit screen
        if (M5.BtnB.wasPressed()) {
            Serial.println("Button B pressed");
            currentScreen = "zip";
            lastScreen = "";  // Force redraw when switching back
        }
        // Go to zip code edit screen
        if (M5.BtnC.wasPressed()) {
            Serial.println("Button C pressed");
            currentScreen = "tempHumidity";
            lastScreen = "";  // Force redraw when switching back
        }
    } 
    else if (currentScreen == "zip") {
        // Ensure screen is redrawn when switching screens
        if (lastScreen != "zip") {
            drawZipCodeScreen();
            lastScreen = "zip";
        }

        // Handle digit changes
        for (int i = 0; i < 5; i++) {
            if (digitUpButtons[i].isPressed()) {
                int digit = zipCode[i] - '0';
                digit = (digit + 1) % 10;
                zipCode[i] = digit + '0';
                drawZipCodeScreen();
                delay(100);
            }

            if (digitDownButtons[i].isPressed()) {
                int digit = zipCode[i] - '0';
                digit = (digit - 1 + 10) % 10;
                zipCode[i] = digit + '0';
                drawZipCodeScreen();
                delay(100);
            }
        }

        // Ensure Button A returns to the weather screen and redraws immediately
        if (M5.BtnA.wasPressed()) {
            Serial.println("Button A pressed - Switching to Weather Screen");
            currentScreen = "weather";
            lastScreen = "";  // Force full refresh of the screen
            M5.Lcd.fillScreen(TFT_NAVY);  // 🔹 Ensure the screen is fully cleared before drawing
            fetchWeatherDetails();
            drawWeatherDisplay();  // 🔹 Redraw everything immediately
        }

        // Save the new zip code and return to weather screen
        if (saveButton.isPressed()) {
            currentScreen = "weather";
            lastScreen = "";  
            M5.Lcd.fillScreen(TFT_NAVY);  // 🔹 Ensure the screen is cleared before updating
            drawWeatherDisplay();  // 🔹 Force a full redraw
        }
    } else if (currentScreen == "tempHumidity") {
        Serial.print("Button C pressed");
        // Ensure screen is redrawn when switching screens
        if (lastScreen != "tempHumidity") {
            drawTempAndHumidity();
            lastScreen = "tempHumidity";
        }
        if (M5.BtnA.wasPressed()) {
            Serial.println("Button A pressed - Switching to Weather Screen");
            currentScreen = "weather";
            lastScreen = "";  // Force full refresh of the screen
            M5.Lcd.fillScreen(TFT_NAVY);  // 🔹 Ensure the screen is fully cleared before drawing
            fetchWeatherDetails();
            drawWeatherDisplay();  // 🔹 Redraw everything immediately
        }
        // Go to zip code edit screen
        if (M5.BtnB.wasPressed()) {
            currentScreen = "zip";
            lastScreen = "";  // Force redraw when switching back
        }
    }

    //---------------VCNL4040 CODE----------------------    
}

