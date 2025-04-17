#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "WiFi.h"
#include "FS.h"                 // SD Card ESP32
#include <EEPROM.h>             // read and write from flash memory
#include <NTPClient.h>          // Time Protocol Libraries
#include <WiFiUdp.h>            // Time Protocol Libraries
#include <Adafruit_VCNL4040.h>  // Sensor libraries
#include "Adafruit_SHT4x.h"     // Sensor libraries
#include <time.h>

///
// TODO 1: Enter your URL addresses///
const String URL_GCF_UPLOAD = "https://timo-cloud-function-service-655170129266.us-west2.run.app";
const String URL_GCF_AVG = "https://timo-get-avg-firestore-function-655170129266.us-central1.run.app";///
// Variables//////
// TODO 2: Enter your WiFi Credentials///
const bool SD_CARD_AND_GCS_UPLOAD_ENABLED = false; // Set to true if you have an SD card and want to upload to GCS
String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "L@ncerN@tion";
String userId = "VincentID"; // Dummy User ID
String selectedUserId = "VincentID";
int selectedDuration = 15;
String selectedDataType = "temp";
const String userOptions[] = {"TimoID", "VincentID", "All"};
const int durationOptions[] = {15, 30, 60};
const String dataTypeOptions[] = {"temp", "rHum", "lux"};
int userIndex = 0;
int durationIndex = 0;
int dataTypeIndex = 0;


enum ScreenMode {
    MAIN_SCREEN,
    DATA_AVG_CONFIG_SCREEN,
    DATA_AVG_RESULT_SCREEN
  };
  
ScreenMode currentScreen = MAIN_SCREEN;
  

// Initialize library objects (sensors and Time protocols)
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelayMs = 2000; 
///
// TODO 3: Device Details Structure///
struct deviceDetails {
    int prox;
    int ambientLight;
    int whiteLight;
    double rHum;
    double temp;
    double accX;
    double accY;
    double accZ;
};
///
// Method header declarations///
int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders);
bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details);
String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details);
int httpPostFile(String serverURL, String *headerKeys, String *headerVals, int numHeaders, String filePath);
bool gcfPostFile(String serverUrl, String filePathOnSD, String userId, time_t time, deviceDetails *details);
String writeDataToFile(byte * fileData, size_t fileSizeInBytes);
int getNextFileNumFromEEPROM();
double convertFintoC(double f);
double convertCintoF(double c);
void updateDisplay(String response);
void showConfigScreen();
void displayAveragingResults(String json);
void showDataAvgConfigScreen();
void makeDataAverageRequest();
void showAverageResults(String json);
void loopMainScreen();
String convertUnixToTime(int64_t unixTime);


struct TouchButton {
    int x, y, w, h;
    String label;
    bool selected;
  };
  
  TouchButton userButtons[3];
  TouchButton durationButtons[3];
  TouchButton typeButtons[3];
  TouchButton getAvgButton;
  
  void drawTouchButton(TouchButton &btn, uint16_t color, bool selected = false) {
    M5.Lcd.fillRect(btn.x, btn.y, btn.w, btn.h, selected ? color : DARKGREY);
    M5.Lcd.drawRect(btn.x, btn.y, btn.w, btn.h, WHITE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(btn.x + 8, btn.y + 8);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("%s", btn.label.c_str());
  }
  


/////////
// Put your setup code here, to run once/////////
void setup()
{
/////
    // Initialize the device
/////
    M5.begin();
    M5.IMU.Init();
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(GREEN);

/////
    // Initialize Sensors
/////
    // Initialize VCNL4040
    if (!vcnl4040.begin()) {
        Serial.println("Couldn't find VCNL4040 chip");
        while (1) delay(1);
    }
    Serial.println("Found VCNL4040 chip");

    // Initialize SHT40
    if (!sht4.begin())
    {
        Serial.println("Couldn't find SHT4x");
        while (1) delay(1);
    }
    Serial.println("Found SHT4x sensor");
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

/////
    // Connect to WiFi
/////
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());

/////
    // Init time connection
/////
    timeClient.begin();
    timeClient.setTimeOffset(3600 * -7);
}

// Call once in setup()
void setupTime() {
    configTime(0, 0, "pool.ntp.org");
    Serial.println("Waiting for NTP time sync...");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("Time synchronized!");
}/////////

// Put your main code here, to run repeatedly/////////
unsigned long lastUpdateTime = 0;  // To store the last update time
const unsigned long updateInterval = 5000;  // 5 seconds in milliseconds

void loop() {
    static unsigned long lastButtonPress = 0;
    const unsigned long debounceDelay = 300;
    M5.update();  // Capture button & touch input

    // Button A: Go back to MAIN_SCREEN from any screen
    if (M5.BtnA.wasPressed() && millis() - lastButtonPress > debounceDelay) {
        lastButtonPress = millis();
        currentScreen = MAIN_SCREEN;
        return;
    }

    // Button B: Go to config screen (only from MAIN_SCREEN)
    if (currentScreen == MAIN_SCREEN && M5.BtnB.wasPressed() && millis() - lastButtonPress > debounceDelay) {
        lastButtonPress = millis();
        currentScreen = DATA_AVG_CONFIG_SCREEN;
        showDataAvgConfigScreen();
        return;
    }

    // Button C: Go straight to AVG RESULTS screen
    if (M5.BtnC.wasPressed() && millis() - lastButtonPress > debounceDelay) {
        lastButtonPress = millis();
        currentScreen = DATA_AVG_RESULT_SCREEN;
        makeDataAverageRequest();
        return;
    }


    // MAIN_SCREEN: run sensor logic
     // Refresh main screen with new data every 5 seconds
     if (currentScreen == MAIN_SCREEN) {
        unsigned long currentMillis = millis();  // Get the current time

        // Check if 5 seconds have passed
        if (currentMillis - lastUpdateTime >= updateInterval) {
            lastUpdateTime = currentMillis;  // Update the last update time
            loopMainScreen();  // Refresh data and update the display
        }
    }

    // Touch interaction (only on config screen)
    if (currentScreen == DATA_AVG_CONFIG_SCREEN) {
        if (M5.Touch.ispressed()) {
            TouchPoint_t tp = M5.Touch.getPressPoint();

            for (int i = 0; i < 3; i++) {
                if (tp.x > userButtons[i].x && tp.x < userButtons[i].x + userButtons[i].w &&
                    tp.y > userButtons[i].y && tp.y < userButtons[i].y + userButtons[i].h) {
                    selectedUserId = userOptions[i];
                    showDataAvgConfigScreen();
                    delay(300); return;
                }

                if (tp.x > durationButtons[i].x && tp.x < durationButtons[i].x + durationButtons[i].w &&
                    tp.y > durationButtons[i].y && tp.y < durationButtons[i].y + durationButtons[i].h) {
                    selectedDuration = durationOptions[i];
                    showDataAvgConfigScreen();
                    delay(300); return;
                }

                if (tp.x > typeButtons[i].x && tp.x < typeButtons[i].x + typeButtons[i].w &&
                    tp.y > typeButtons[i].y && tp.y < typeButtons[i].y + typeButtons[i].h) {
                    selectedDataType = dataTypeOptions[i];
                    showDataAvgConfigScreen();
                    delay(300); return;
                }
            }

            if (tp.x > getAvgButton.x && tp.x < getAvgButton.x + getAvgButton.w &&
                tp.y > getAvgButton.y && tp.y < getAvgButton.y + getAvgButton.h) {
                currentScreen = DATA_AVG_RESULT_SCREEN;
                makeDataAverageRequest();
                delay(300); return;
            }
        }
    }
}

      

void loopMainScreen() {
/////
    // Read Sensor Values
/////
    // Read VCNL4040 Sensors
    Serial.printf("Live/local sensor readings:\n");
    uint16_t prox = vcnl4040.getProximity();
    uint16_t ambientLight = vcnl4040.getLux();
    uint16_t whiteLight = vcnl4040.getWhiteLight();
    Serial.printf("\tProximity: %d\n", prox);
    Serial.printf("\tAmbient light: %d\n", ambientLight);
    Serial.printf("\tRaw white light: %d\n", whiteLight);

    // Read SHT40 Sensors
    sensors_event_t rHum, temp;
    sht4.getEvent(&rHum, &temp); // populate temp and humidity objects with fresh data
    Serial.printf("\tTemperature: %.2fF\n", convertCintoF(temp.temperature));
    Serial.printf("\tHumidity: %.2f %%rH\n", rHum.relative_humidity);

    // Read M5's Internal Accelerometer (MPU 6886)
    float accX;
    float accY;
    float accZ;
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    accX *= 9.8;
    accY *= 9.8;
    accZ *= 9.8;
    Serial.printf("\tAccel X=%.2fm/s^2\n", accX);        
    Serial.printf("\tAccel Y=%.2fm/s^2\n", accY);
    Serial.printf("\tAccel Z=%.2fm/s^2\n", accZ);

/////
    // Setup data to upload to Google Cloud Functions
/////
    // Get current time as timestamp of last update
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    unsigned long long epochMillis = ((unsigned long long)epochTime)*1000;
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    Serial.printf("\nCurrent Time:\n\tEpoch (ms): %llu", epochMillis);
    Serial.printf("\n\tFormatted: %d/%d/%d ", ptm->tm_mon+1, ptm->tm_mday, ptm->tm_year+1900);
    Serial.printf("%02d:%02d:%02d%s\n\n", timeClient.getHours() % 12, timeClient.getMinutes(), timeClient.getSeconds(), timeClient.getHours() < 12 ? "AM" : "PM");
    
    // Device details
    deviceDetails details;
    details.prox = prox;
    details.ambientLight = ambientLight;
    details.whiteLight = whiteLight;
    details.temp = temp.temperature;
    details.rHum = rHum.relative_humidity;
    details.accX = accX;
    details.accY = accY;
    details.accZ = accZ;

/////
    // Post data (and possibly file)
/////
	if (!SD_CARD_AND_GCS_UPLOAD_ENABLED) {
		// Option 1: Just post data	
		gcfGetWithHeader(URL_GCF_UPLOAD, userId, epochTime, &details);
	} else {
		// Option 2: Post data with (dummy) file to upload
		byte dummyFile [] = {'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', 'S', 'D'};
		String filePath = writeDataToFile(dummyFile, sizeof(dummyFile));
		gcfPostFile(URL_GCF_UPLOAD, filePath, userId, epochTime, &details);
    }

/////
    // Update Display
/////
    // Make GET request and pass response to the new updateDisplay
    HTTPClient http;
    http.begin("https://timo-get-firestore-function-655170129266.us-central1.run.app/");
    int httpCode = http.GET();
    if (httpCode == 200) {
    String payload = http.getString();
    updateDisplay(payload);
    } else {
    Serial.printf("HTTP Error: %d\n", httpCode);
    }
    http.end();


/////
    // Delay
/////
    delay(timerDelayMs);
}
///
// Function to update M5 screen with the latest sensor values///
void updateDisplay(String response) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.f_str());
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.setTextColor(RED);
        M5.Lcd.setTextSize(2);
        M5.Lcd.println("Failed to parse JSON.");
        return;
    }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setTextSize(2);

    JsonObject vcnl = doc["vcnlDetails"];
    JsonObject sht = doc["shtDetails"];
    JsonObject m5Details = doc["m5Details"];
    JsonObject other = doc["otherDetails"];

    int prox = vcnl["prox"];
    int al = vcnl["al"];
    int rwl = vcnl["rwl"];
    float temp = sht["temp"];
    float rHum = sht["rHum"];
    float ax = m5Details["ax"];
    float ay = m5Details["ay"];
    float az = m5Details["az"];
    unsigned long timeCaptured = other["timeCaptured"];

    const char* docId = doc["id"] | "N/A";

    time_t rawTime = (time_t) timeCaptured;
    time_t adjustedTime = rawTime - 2;

    struct tm *ptmRaw = localtime(&rawTime);
    struct tm *ptmAdj = localtime(&adjustedTime);

    int hourAdj = ptmAdj->tm_hour % 12;
    if (hourAdj == 0) hourAdj = 12;
    const char* ampmAdj = ptmAdj->tm_hour < 12 ? "AM" : "PM";

    int hourRaw = ptmRaw->tm_hour % 12;
    if (hourRaw == 0) hourRaw = 12;
    const char* ampmRaw = ptmRaw->tm_hour < 12 ? "AM" : "PM";



    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    
    // Doc ID - WHITE
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(20, 10);
    M5.Lcd.printf("Doc ID:\n%s\n", doc["id"] | "N/A");

    M5.Lcd.printf("POST: %02d:%02d:%02d%s\n", hourAdj, ptmAdj->tm_min, ptmAdj->tm_sec, ampmAdj);
    M5.Lcd.drawLine(0, M5.Lcd.getCursorY(), 320, M5.Lcd.getCursorY(), WHITE);
    
    // Left & Right column anchors
    int leftX = 10;
    int rightX = 145;
    int topY = M5.Lcd.getCursorY() + 10;
    
    // VCNL - RED (left)
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(leftX, topY);
    M5.Lcd.printf("VCNL:\n");
    M5.Lcd.setCursor(leftX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("prox: %d\n", prox);
    M5.Lcd.setCursor(leftX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("al:   %d\n", al);
    M5.Lcd.setCursor(leftX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("rwl:  %d\n", rwl);
    
    // SHT - YELLOW (right)
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.setCursor(rightX, topY);
    M5.Lcd.printf("SHT:\n");
    M5.Lcd.setCursor(rightX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("Temp: %.2f C\n", temp);
    M5.Lcd.setCursor(rightX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("RH:   %.2f%%\n", rHum);
    
    // Divider
    int midY = max(M5.Lcd.getCursorY(), M5.Lcd.getCursorY());
    M5.Lcd.drawLine(0, midY + 20, 320, midY + 20, WHITE);
    
    // Bottom section anchors
    int bottomY = midY + 30;
    
    // M5 Accel - BLUE (left)
    M5.Lcd.setTextColor(BLUE);
    M5.Lcd.setCursor(leftX, bottomY);
    M5.Lcd.printf("M5:\n");
    M5.Lcd.setCursor(leftX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("ax: %.2f\n", ax);
    M5.Lcd.setCursor(leftX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("ay: %.2f\n", ay);
    M5.Lcd.setCursor(leftX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("az: %.2f\n", az);
    
    // OTHER - GREEN (right)
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setCursor(rightX, bottomY);
    M5.Lcd.printf("OTHER:\n");
    M5.Lcd.setCursor(rightX + 10, M5.Lcd.getCursorY());



    
    M5.Lcd.printf("%02d:%02d:%02d%s\n", hourRaw, ptmRaw->tm_min, ptmRaw->tm_sec, ampmRaw);

    M5.Lcd.setCursor(rightX + 10, M5.Lcd.getCursorY());
    M5.Lcd.printf("User: %s\n", userId);    
}

void makeDataAverageRequest() {
    String url = "https://timo-get-avg-655170129266.us-central1.run.app/"
                 "?userId=" + selectedUserId +
                 "&timeDuration=" + String(selectedDuration) +
                 "&dataType=" + selectedDataType;

    Serial.println("Making request to: " + url);  // Debug log

    HTTPClient http;
    http.begin(url); // Pass full URL with params

    int code = http.GET();

    if (code == 200) {
        String payload = http.getString();
        Serial.println("Response: " + payload);
        showAverageResults(payload);  // Show on M5 screen
    } else {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(RED);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.setTextSize(2);
        M5.Lcd.printf("HTTP ERROR %d", code);
    }

    http.end();
}



void showDataAvgConfigScreen() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Tap to select params");
  
    // --- User ID Buttons
    for (int i = 0; i < 3; i++) {
      userButtons[i] = {10 + i * 100, 40, 90, 40, userOptions[i], selectedUserId == userOptions[i]};
      drawTouchButton(userButtons[i], GREEN, userButtons[i].selected);
    }
  
    // --- Time Duration Buttons
    for (int i = 0; i < 3; i++) {
      durationButtons[i] = {10 + i * 100, 100, 90, 40, String(durationOptions[i]) + "s", selectedDuration == durationOptions[i]};
      drawTouchButton(durationButtons[i], YELLOW, durationButtons[i].selected);
    }
  
    // --- Data Type Buttons
    for (int i = 0; i < 3; i++) {
      typeButtons[i] = {10 + i * 100, 160, 90, 40, dataTypeOptions[i], selectedDataType == dataTypeOptions[i]};
      drawTouchButton(typeButtons[i], CYAN, typeButtons[i].selected);
    }
  
    // --- Get Average Button
    getAvgButton = {60, 220, 200, 40, "Get Average", false};
    drawTouchButton(getAvgButton, RED);
  }
  

void showAverageResults(String json) {
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, json);

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);

    if (err) {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("JSON Parse Error");
        return;
    }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setTextSize(2);
    
    String dataType = doc["dataType"] | "N/A";
    float avg = doc["averageValue"] | 0.0;
    int minTime = doc["minTime"] | 0;
    int maxTime = doc["maxTime"] | 0;
    int secondsElapsed = doc["secondsElapsed"] | 0;
    int numPoints = doc["numPoints"] | 0;
    float rate = doc["rate"] | 0.0;
    
    // Convert UNIX timestamps to readable date/time (optional - stubbed here)
    String minTimeStr = convertUnixToTime(minTime); // Implement this
    String maxTimeStr = convertUnixToTime(maxTime); // Implement this
    
    M5.Lcd.printf("Sensor: %s\n", dataType.c_str());
    M5.Lcd.printf("Average: %.2f\n", avg);
    M5.Lcd.printf("Range:\n%s –\n%s (%ds)\n",
                  minTimeStr.c_str(), maxTimeStr.c_str(), secondsElapsed);
    M5.Lcd.printf("Points: %d datapoints\n", numPoints);
    M5.Lcd.printf("Rate: %.2f datapoints/s\n", rate);
    
}

String convertUnixToTime(int64_t unixTime) {
    time_t raw = (time_t)unixTime;
    struct tm *timeInfo = gmtime(&raw);  // UTC-based conversion
    char buf[32];
    strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", timeInfo);
    return String(buf);
}






  
///
// This method takes in a user ID, time and structure describing
// device details and makes a GET request with the data. ///
bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details) {
    // Allocate arrays for headers
	const int numHeaders = 1;
    String headerKeys [numHeaders] = {"M5-Details"};
    String headerVals [numHeaders];

    // Add formatted JSON string to header
    headerVals[0] = generateM5DetailsHeader(userId, time, details);
    
    // Attempt to post the file
    Serial.println("Attempting post data.");
    int resCode = httpGetWithHeaders(serverUrl, headerKeys, headerVals, numHeaders);
    
    // Return true if received 200 (OK) response
    return (resCode == 200);
}
///
// TODO 4: Implement function
// Generates the JSON header with all the sensor details and user
// data and serializes to a String.///
String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details) {
    // Allocate M5-Details Header JSON object
    StaticJsonDocument<650> objHeaderM5Details; //DynamicJsonDocument  objHeaderGD(600);
    
    // Add top-level field: timeCaptured
    objHeaderM5Details["timeCaptured"] = time;
    
    // Add VCNL details
    JsonObject objVcnlDetails = objHeaderM5Details.createNestedObject("vcnlDetails");
    objVcnlDetails["prox"] = details->prox;
    objVcnlDetails["al"] = details->ambientLight;
    objVcnlDetails["rwl"] = details->whiteLight;

    // Add SHT details
    JsonObject objShtDetails = objHeaderM5Details.createNestedObject("shtDetails");
    objShtDetails["temp"] = details->temp;
    objShtDetails["rHum"] = details->rHum;

    // Add M5 Sensor details
    JsonObject objM5Details = objHeaderM5Details.createNestedObject("m5Details");
    objM5Details["ax"] = details->accX;
    objM5Details["ay"] = details->accY;
    objM5Details["az"] = details->accZ;

    // Add Other details
    JsonObject objOtherDetails = objHeaderM5Details.createNestedObject("otherDetails");
    objOtherDetails["timeCaptured"] = time;
    objOtherDetails["userId"] = userId;

    // Convert JSON object to a String which can be sent in the header
    size_t jsonSize = measureJson(objHeaderM5Details) + 1;
    char cHeaderM5Details [jsonSize];
    serializeJson(objHeaderM5Details, cHeaderM5Details, jsonSize);
    String strHeaderM5Details = cHeaderM5Details;
    //Serial.println(strHeaderM5Details.c_str()); // Debug print

    // Return the header as a String
    return strHeaderM5Details;
}
///
// This method takes in a serverURL and array of headers and makes
// a GET request with the headers attached and then returns the response.///
int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders) {
    // Make GET request to serverURL
    HTTPClient http;
    http.begin(serverURL.c_str());
    
///
	// TODO 5: Add all the headers supplied via parameter
///
    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    
    // Post the headers (NO FILE)
    int httpResCode = http.GET();

    // Print the response code and message
    Serial.printf("HTTP%scode: %d\n%s\n\n", httpResCode > 0 ? " " : " error ", httpResCode, http.getString().c_str());

    // Free resources and return response code
    http.end();
    return httpResCode;
}
// TODO 6: Implement method
// Given an array of bytes, writes them out to the SD file system.///
String writeDataToFile(byte * fileData, size_t fileSizeInBytes) {
    // Print status
    Serial.println("Attempting to write file to SD Card...");

    // Obtain file system from SD card
    fs::FS &sdFileSys = SD;

    // Generate path where new picture will be saved on SD card and open file
    int fileNumber = getNextFileNumFromEEPROM();
    String path = "/file_" + String(fileNumber) + ".txt";
    File file = sdFileSys.open(path.c_str(), FILE_WRITE);

    // If file was opened successfully
    if (file)
    {
        // Write image bytes to the file
        Serial.printf("\tSTATUS: %s FILE successfully OPENED\n", path.c_str());
        file.write(fileData, fileSizeInBytes); // payload (file), payload length
        Serial.printf("\tSTATUS: %s File successfully WRITTEN (%d bytes)\n\n", path.c_str(), fileSizeInBytes);

        // Update picture number
        EEPROM.write(0, fileNumber);
        EEPROM.commit();
    }
    else {
        Serial.printf("\t***ERROR: %s file FAILED OPEN in writing mode\n***", path.c_str());
        return "";
    }

    // Close file
    file.close();

    // Return file name
    return path;
}
///
// TODO 7: Implement Method
// Get file number by reading last file number in EEPROM (non-volatile
// memory area).///
int getNextFileNumFromEEPROM() {
    #define EEPROM_SIZE 1             // Number of bytes you want to access
    EEPROM.begin(EEPROM_SIZE);
    int fileNumber = 0;               // Init to 0 in case read fails
    fileNumber = EEPROM.read(0) + 1;  // Variable to represent file number
    return fileNumber;
}
///
// TODO 8: Implement Method
// This method takes in an SD file path, user ID, time and structure
// describing device details and POSTs it. ///
bool gcfPostFile(String serverUrl, String filePathOnSD, String userId, time_t time, deviceDetails *details) {
    // Allocate arrays for headers
    const int numHeaders = 3;
    String headerKeys [numHeaders] = { "Content-Type", "Content-Disposition", "M5-Details"};
    String headerVals [numHeaders];

    // Content-Type Header
    headerVals[0] = "text/plain";
    
    // Content-Disposition Header
    String filename = filePathOnSD.substring(filePathOnSD.lastIndexOf("/") + 1);
    String headerCD = "attachment; filename=" + filename;
    headerVals[1] = headerCD;

    // Add formatted JSON string to header
    headerVals[2] = generateM5DetailsHeader(userId, time, details);
    
    // Attempt to post the file
    int numAttempts = 1;
    Serial.printf("Attempting upload of %s...\n", filename.c_str());
    int resCode = httpPostFile(serverUrl, headerKeys, headerVals, numHeaders, filePathOnSD);
    
    // If first attempt failed, retry...
    while (resCode != 200) {
        // ...up to 9 more times (10 tries in total)
        if (++numAttempts >= 10)
            break;

        // Re-attempt
        Serial.printf("*Re-attempting upload (try #%d of 10 max tries) of %s...\n", numAttempts, filename.c_str());
        resCode = httpPostFile(serverUrl, headerKeys, headerVals, numHeaders, filePathOnSD);
    }

    // Return true if received 200 (OK) response
    return (resCode == 200);
}
///
// TODO 9: Implement Method
// This method takes in a serverURL and file path and makes a 
// POST request with the file (to upload) and then returns the response.///
int httpPostFile(String serverURL, String *headerKeys, String *headerVals, int numHeaders, String filePath) {
    // Make GET request to serverURL
    HTTPClient http;
    http.begin(serverURL.c_str());
    
    // Add all the headers supplied via parameter
    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    
    // Open the file, upload and then close
    fs::FS &sdFileSys = SD;
    File file = sdFileSys.open(filePath.c_str(), FILE_READ);
    int httpResCode = http.sendRequest("POST", &file, file.size());
    file.close();

    // Print the response code and message
    Serial.printf("\tHTTP%scode: %d\n\t%s\n\n", httpResCode > 0 ? " " : " error ", httpResCode, http.getString().c_str());

    // Free resources and return response code
    http.end();
    return httpResCode;
}

// Convert between F and C temperatures
double convertFintoC(double f) { return (f - 32) * 5.0 / 9.0; }
double convertCintoF(double c) { return (c * 9.0 / 5.0) + 32; }