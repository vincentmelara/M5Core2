#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include "EGR425_Phase1_weather_bitmap_images.h"
#include "WiFi.h"
#include "I2C_RW.h"
#include "I2C_SHT.h"

// Variables
int const PIN_SDA = 32;
int const PIN_SCL = 33;

// Register defines
#define VCNL_I2C_ADDRESS 0x60
#define SHT40_I2C_ADDRESS 0x44

#define VCNL_REG_PROX_DATA 0x08
#define VCNL_REG_ALS_DATA 0x09
#define VCNL_REG_WHITE_DATA 0x0A
#define SHT40_REG_TEMPHUMI 0xFD

#define VCNL_REG_PS_CONFIG 0x03
#define VCNL_REG_ALS_CONFIG 0x00
#define VCNL_REG_WHITE_CONFIG 0x04


//Register for openweather account and get API key
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "234a0b7be99d32d485d764d1c7e3246e";

//WiFi variables
String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "L@ncerN@tion";

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;  // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)

// LCD variables
int sWidth;
int sHeight;
bool isLocal = false;
bool isFahrenheit = true;
bool showingHumidity = false;
bool isEditingZipCode = false;
bool showingProximity = false;

// Method header declarations
String httpGETRequest(const char* serverName);
void displayTemperature(double tempNow, double tempMin, double tempMax, String cityName, uint16_t primaryTextColor, String formattedTime, float liveHumiPercent);
void drawOutlinedText(int x, int y, const char* text, uint16_t textColor, uint16_t outlineColor, int textSize);
void updateLocalTemperatureDisplay(uint16_t backgroundColor);
void controlLCD(uint16_t ambientLight, uint16_t proximity);
void updateTemperatureDisplay(uint16_t backgroundColor);
void drawWeatherImage(String iconId, int resizeMult);
void drawZipCodeScreen();
void updateMainScreen(uint16_t backgroundColor);
void initVCNL4040();
void initSHT40();

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

uint16_t readVCNLProximity();
uint16_t readVCNLAmbientLight();
uint32_t readSHT40TemperatureHumidity();

// Time and NTP setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -8 * 3600, 60000); // Adjust the timezone offset to PST


// Put your setup code here, to run once
void setup() {
    // Initialize the device
    M5.begin();
    M5.Lcd.setRotation(1);
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

    // Initialize NTPClient
    timeClient.begin();

    initVCNL4040();
    initSHT40();
}

// Put your main code here, to run repeatedly
void loop() {
    M5.update();
    static String lastScreen = "";  // Track the last screen to prevent unnecessary redraws

    // Check if Button A is pressed to toggle temperature unit
    if (M5.BtnA.wasPressed()) {
        isFahrenheit = !isFahrenheit;
        String strWeatherIcon = "";
        uint16_t backgroundColor = (strWeatherIcon.indexOf("d") >= 0) ? TFT_CYAN : TFT_NAVY;
        updateMainScreen(backgroundColor);
        M5.update();
    }
    // Check if Button B is pressed to toggle zip code editing mode
    if (M5.BtnB.wasPressed()) {
        isEditingZipCode = !isEditingZipCode;
        String strWeatherIcon = "";
        uint16_t backgroundColor = (strWeatherIcon.indexOf("d") >= 0) ? TFT_CYAN : TFT_NAVY;
        drawZipCodeScreen();
        
        while (true)
        {
            M5.update();
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

            // Save the new zip code and return to weather screen
            if (saveButton.isPressed()) {
                M5.Lcd.fillScreen(TFT_BLACK);  // 🔹 Ensure the screen is cleared before updating
                updateMainScreen(backgroundColor);
            }
                // Read the sensors
            uint16_t proxValue = readVCNLProximity();
            uint16_t alsValue = readVCNLAmbientLight();

            // Dim the LCD based on ambient light
            controlLCD(alsValue, proxValue);

            if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
                break;
            }
        }

        M5.update();
        updateMainScreen(backgroundColor);
    }
    
    // Check if Button C is pressed to exit zip code editing mode
    if (M5.BtnC.wasPressed()) {
        isLocal = !isLocal;
        showingHumidity = !showingHumidity;
        String strWeatherIcon = "";
        uint16_t backgroundColor = (strWeatherIcon.indexOf("d") >= 0) ? TFT_CYAN : TFT_NAVY;
        updateMainScreen(backgroundColor);
        M5.update();
    }

    // Only execute every so often
    if ((millis() - lastTime) > timerDelay) {
        String strWeatherIcon = "";
        uint16_t backgroundColor = (strWeatherIcon.indexOf("d") >= 0) ? TFT_CYAN : TFT_NAVY;
        updateMainScreen(backgroundColor);

        lastTime = millis();
    }

        // Read the sensors
        uint16_t proxValue = readVCNLProximity();
        uint16_t alsValue = readVCNLAmbientLight();
    
        // Dim the LCD based on ambient light
        controlLCD(alsValue, proxValue);
}

// Updates the temperature display after toggling Fahrenheit/Celsius
void updateTemperatureDisplay(uint16_t backgroundColor) {
    
    // Fetch latest weather data
    if (WiFi.status() == WL_CONNECTED) {
        String serverURL = urlOpenWeather + "zip=" + zipCode + ",us&units=imperial&appid=" + apiKey;
        String response = httpGETRequest(serverURL.c_str());

        DynamicJsonDocument objResponse(1024);
        DeserializationError error = deserializeJson(objResponse, response);
        if (error) return;

        String strWeatherIcon = objResponse["weather"][0]["icon"];
        String cityName = objResponse["name"];
        double tempNow = objResponse["main"]["temp"];
        double tempMin = objResponse["main"]["temp_min"];
        double tempMax = objResponse["main"]["temp_max"];

        if (!isFahrenheit) {
            tempNow = (tempNow - 32) * 5.0 / 9.0;
            tempMin = (tempMin - 32) * 5.0 / 9.0;
            tempMax = (tempMax - 32) * 5.0 / 9.0;
        }

        uint16_t primaryTextColor = (strWeatherIcon.indexOf("d") >= 0) ? TFT_DARKGREY : TFT_WHITE;
        M5.Lcd.fillScreen(backgroundColor);
        drawWeatherImage(strWeatherIcon, 2);
        timeClient.update();
        String formattedTime = timeClient.getFormattedTime();
        int hour = formattedTime.substring(0, 2).toInt();
        String meridiem = " AM";
        if (hour >= 12) {
            meridiem = " PM";
            if (hour > 12) hour -= 12;
        } else if (hour == 0) {
            hour = 12;
        }
        formattedTime = String(hour) + formattedTime.substring(2) + meridiem;
        displayTemperature(tempNow, tempMin, tempMax, cityName, primaryTextColor, formattedTime, 0);
    }
}

// This method updates the local temperature display
void updateLocalTemperatureDisplay(uint16_t backgroundColor) {

    // Read the sensors
    uint32_t tempHumiValue = readSHT40TemperatureHumidity();
    uint16_t liveTempRaw = tempHumiValue >> 16;
    uint16_t liveHumiRaw = tempHumiValue & 0xFFFF;
    float liveTempCelsius = -45.0 + 175.0 * liveTempRaw / 65535.0;
    float liveHumiPercent = -6.0 + 125.0 * liveHumiRaw / 65535.0;

    // Store high and low temperatures
    static float highTempCelsius = 24;
    static float lowTempCelsius = 24;

    if (liveTempCelsius > highTempCelsius) {
        highTempCelsius = liveTempCelsius;
    }
    if (liveTempCelsius < lowTempCelsius) {
        lowTempCelsius = liveTempCelsius;
    }

    if (WiFi.status() == WL_CONNECTED) {
        String serverURL = urlOpenWeather + "zip=" + zipCode + ",us&units=imperial&appid=" + apiKey;
        String response = httpGETRequest(serverURL.c_str());
        
        DynamicJsonDocument objResponse(1024);
        DeserializationError error = deserializeJson(objResponse, response);
        if (error) return;

        String strWeatherIcon = objResponse["weather"][0]["icon"];
        String cityName = "";
        double tempNow = liveTempCelsius;
        double tempMin = lowTempCelsius;
        double tempMax = highTempCelsius;

        if (isFahrenheit) {
            tempNow = tempNow * 9.0 / 5.0 + 32.0;
            tempMin = tempMin * 9.0 / 5.0 + 32.0;
            tempMax = tempMax * 9.0 / 5.0 + 32.0;
        }

        uint16_t primaryTextColor = (strWeatherIcon.indexOf("d") >= 0) ? TFT_DARKGREY : TFT_WHITE;
        M5.Lcd.fillScreen(backgroundColor);
        drawWeatherImage(strWeatherIcon, 2);
        timeClient.update();
        String formattedTime = timeClient.getFormattedTime();
        int hour = formattedTime.substring(0, 2).toInt();
        String meridiem = " AM";
        if (hour >= 12) {
            meridiem = " PM";
            if (hour > 12) hour -= 12;
        } else if (hour == 0) {
            hour = 12;
        }
        formattedTime = String(hour) + formattedTime.substring(2) + meridiem + " PST";
        displayTemperature(tempNow, tempMin, tempMax, cityName, primaryTextColor, formattedTime, liveHumiPercent);
    }
}

// Updates the main screen after leaving zip code editing mode 
void updateMainScreen(uint16_t backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    if (showingHumidity)
    {
        updateLocalTemperatureDisplay(backgroundColor);
    }
    else
    {
        updateTemperatureDisplay(backgroundColor);
    }
    // Read the sensors
    uint16_t proxValue = readVCNLProximity();
    uint16_t alsValue = readVCNLAmbientLight();

    controlLCD(alsValue, proxValue);
}
//
// This method takes in a URL and makes a GET request to the
// URL, returning the response.//
String httpGETRequest(const char* serverURL) {
    
    // Initialize client
    HTTPClient http;
    http.begin(serverURL);

    // Send HTTP GET request and obtain response
    int httpResponseCode = http.GET();
    String response = http.getString();

    // Check if got an error
    if (httpResponseCode > 0)
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    else {
        Serial.printf("HTTP Response ERROR code: %d\n", httpResponseCode);
        Serial.printf("Server Response: %s\n", response);
    }

    // Free resources and return response
    http.end();
    return response;
}

void drawWeatherImage(String iconId, int resizeMult) {

    // Get the corresponding byte array
    const uint16_t * weatherBitmap = getWeatherBitmap(iconId);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    //int yOffset = -(resizeMult * imgSqDim - M5.Lcd.height()) / 2;
    int yOffset = (M5.Lcd.height() / 2) - (imgSqDim * resizeMult / 2);
    //int xOffset = sWidth - (imgSqDim*resizeMult*.8); // Right align (image doesn't take up entire array)
    int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally
    
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
//
// This method draws text with an outline//
void drawOutlinedText(int x, int y, const char* text, uint16_t textColor, uint16_t outlineColor, int textSize) {
    M5.Lcd.setTextSize(textSize);
    M5.Lcd.setTextColor(outlineColor);
    M5.Lcd.setCursor(x - 1, y);
    M5.Lcd.print(text);
    M5.Lcd.setCursor(x + 1, y);
    M5.Lcd.print(text);
    M5.Lcd.setCursor(x, y - 1);
    M5.Lcd.print(text);
    M5.Lcd.setCursor(x, y + 1);
    M5.Lcd.print(text);
    M5.Lcd.setTextColor(textColor);
    M5.Lcd.setCursor(x, y);
    M5.Lcd.print(text);
}
//
// This method displays the temperature on the screen//
void displayTemperature(double tempNow, double tempMin, double tempMax, String cityName, uint16_t primaryTextColor, String formattedTime, float liveHumiPercent) {
    int pad = 10;
    int screenWidth = M5.Lcd.width();  // Get screen width
    int maxTextWidth = screenWidth - 20;  // Max text width (with padding)

    if (showingHumidity) {
        M5.Lcd.fillScreen(TFT_BLACK);
    
        // Display humidity text
        M5.Lcd.setCursor(pad, pad);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), ("Humidity: " + String(liveHumiPercent, 1) + "%").c_str(), primaryTextColor, TFT_BLACK, 3);
    
        // Loading Bar Settings
        int barWidth = M5.Lcd.width() - (2 * pad);  // Full width minus padding
        int barHeight = 20;  // Height of the loading bar
        int barX = pad;
        int barY = M5.Lcd.getCursorY() + 35;  // Place below humidity text
    
        // Draw Empty Bar Outline
        M5.Lcd.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);
    
        // Calculate the fill width based on humidity percentage
        int fillWidth = (barWidth * liveHumiPercent) / 100;
    
        // Draw the filled part of the bar
        M5.Lcd.fillRect(barX + 1, barY + 1, fillWidth - 2, barHeight - 2, TFT_GREEN);
    
        // Display temperature values
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 65);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), ("local temp: " +  String(tempNow, 1) + (isFahrenheit ? "F" : "C")).c_str(), primaryTextColor, TFT_BLACK, 3);
    
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 25);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), ("LO:" + String(tempMin, 1) + (isFahrenheit ? "F" : "C")).c_str(), TFT_BLUE, TFT_BLACK, 3);
        
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 25);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), ("HI:" + String(tempMax, 1) + (isFahrenheit ? "F" : "C")).c_str(), TFT_RED, TFT_BLACK, 3);
    }
    
    else{
        M5.Lcd.setCursor(pad, pad);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), (String(tempNow, 1) + (isFahrenheit ? "F" : "C")).c_str(), primaryTextColor, TFT_BLACK, 6);

        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 55);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), ("LO:" + String(tempMin, 1) + (isFahrenheit ? "F" : "C")).c_str(), TFT_BLUE, TFT_BLACK, 3);

        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 25);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), ("HI:" + String(tempMax, 1) + (isFahrenheit ? "F" : "C")).c_str(), TFT_RED, TFT_BLACK, 3);
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 25);
        drawOutlinedText(pad, M5.Lcd.getCursorY(), ("Last Sync:" + formattedTime).c_str(), primaryTextColor, TFT_BLACK, 2);

    }
    // City Name Handling - No Centering, but Wrapping
    String firstLine = cityName;
    String secondLine = "";

    int cityWidth = M5.Lcd.textWidth(firstLine, 3); // Use text size 3 for city name
    if (cityWidth > maxTextWidth) {
        int splitIndex = firstLine.lastIndexOf(" ");  // Split at last space
        if (splitIndex != -1) {
            firstLine = cityName.substring(0, splitIndex);
            secondLine = cityName.substring(splitIndex + 1);
        }
    }

    // Display the first line of city name (left-aligned)
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 30);
    drawOutlinedText(pad, M5.Lcd.getCursorY(), firstLine.c_str(), primaryTextColor, TFT_BLACK, 3);

    // Display the second line of city name (if any)
    if (secondLine.length() > 0) {
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY() + 30); // Position the second line below the first
        drawOutlinedText(pad, M5.Lcd.getCursorY(), secondLine.c_str(), primaryTextColor, TFT_BLACK, 3);
    }

}

// This method displays the zip code screen//
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


// This method initializes the VCNL4040 proximity sensor//
void initSHT40() {
    // Connect to SHT40 sensor
    I2C_SHT::initI2C(SHT40_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);
    //I2C_RW::scanI2cLinesForAddresses(false);

    Serial.println("SHT40: Temperature and humidity sensors configured!");
}

uint32_t readSHT40TemperatureHumidity() {
    // I2C call to read sensor temperature data and print
    uint32_t temp = I2C_SHT::readReg8Addr48Data(SHT40_REG_TEMPHUMI, 6, "to read temperature data", false);
    
    //only taking 16 bits for the temperature
    uint16_t liveTempRaw = temp >> 16;

    //we all bits for the humidity value
    uint16_t liveHumiRaw = temp & 0xFFFF;


    //splits up the data 
    float liveTemp = -45.0 + 175.0 * liveTempRaw / 65535.0;
    float liveHumi = -6.0 + 125.0 * liveHumiRaw / 65535.0;
    
    Serial.printf("Temperature: %.2f C\n", liveTemp);
    Serial.printf("Humidity: %.2f%%\n", liveHumi);
    Serial.printf("\n");

    return temp;
}

// This method initializes the VCNL4040 proximity sensor
void initVCNL4040() {
    
    // Connect to VCNL sensor
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);
    //I2C_RW::scanI2cLinesForAddresses(false);

	// Write registers to initialize/enable VCNL sensors
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_WHITE_CONFIG, 2, 0x0000, " to enable raw white light sensor", true);
    Serial.println("VCNL4040: Proximity sensor configured!");
}


uint16_t readVCNLProximity() {
// readReg8Addr16Data(registerAddress, numBytes, actionName, verbose)
      // I2C call to read sensor proximity data and print
      int prox = I2C_RW::readReg8Addr16Data(VCNL_REG_PROX_DATA, 2, "to read proximity data", false);
      Serial.printf("Proximity: %d\n", prox);
  return prox;
}


uint16_t readVCNLAmbientLight() {
// I2C call to read sensor ambient light data and print
       int als = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
       als = als * 0.1; // See pg 12 of datasheet - we are using ALS_IT (7:6)=(0,0)=80ms integration time = 0.10 lux/step for a max range of 6553.5 lux
       Serial.printf("Ambient Light: %d\n", als);
  return als;
}

// These methods dims or turn offthe LCD based on ambient light or proximity
void controlLCD(uint16_t ambientLight, uint16_t proximity) {
    static uint16_t lastBrightness = 0;  
    if (proximity > 200) { 
        M5.Axp.SetLcdVoltage(2500);
        M5.Lcd.sleep();
    } else {
        M5.Lcd.wakeup();
        uint16_t brightness = map(ambientLight, 0, 6553, 3300, 2500); 
        if (brightness != lastBrightness) { // Only update if changed
            M5.Axp.SetLcdVoltage(brightness);
            lastBrightness = brightness;
        }
    }
}


