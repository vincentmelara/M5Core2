// Some useful resources on BLE and ESP32:
//      https://github.com/nkolban/ESP32_BLE_Arduino/blob/master/examples/BLE_notify/BLE_notify.ino
//      https://microcontrollerslab.com/esp32-bluetooth-low-energy-ble-using-arduino-ide/
//      https://randomnerdtutorials.com/esp32-bluetooth-low-energy-ble-arduino-ide/
//      https://www.electronicshub.org/esp32-ble-tutorial/

//core libraries for bluetooth on ESP32
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <M5Core2.h>
//
#include <BLEServer.h>


// Variables

//bluetooth profile will have a service, and the service can have multiple characteristics
//each characterisitic has properties, a value, and a descriptor
BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *bleCharacteristic;

//used in callback methods
bool deviceConnected = false;

//init timer variable for use in loop
int timer = 0;

// See the following for generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE Server Callback Methods

//create a class that inherits from BLEServerCallbacks
/*here we inherit from the Callback class to make it easier to implement callbacks*/
class MyServerCallbacks: public BLEServerCallbacks {
    //here we override the original methods to handle when a user connects/disconnects
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        Serial.println("Device connected...");
    }
    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

// Put your setup code here, to run once
void setup()
{
    // Init device
    M5.begin();
    Serial.print("Starting BLE...");

    // Initialize M5Core2 as a BLE server
    BLEDevice::init("Vincent Melara's M5Core2");

    //here we create the server/profile for our BLEDevice
    bleServer = BLEDevice::createServer();

    //after creating callback methods we can add them here to our server 
    //by passing in our class to the built in setcallbacks method
    bleServer->setCallbacks(new MyServerCallbacks());

    //here we create a service where we pass in our service UUID that is added to our server
    bleService = bleServer->createService(SERVICE_UUID);

    //here we attach a characteristic to our service 
    //it takes in the UUID of the characteristic and the properties used by the service
    bleCharacteristic = bleService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );

    //set initial value for hello world
    bleCharacteristic->setValue("Hello BLE World from Vincent!");

    //start the service
    bleService->start();

    // Start broadcasting (advertising) BLE service

    //static method that grabs the devices bluetooth capabilities
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();

    //from the above object we can add a service UUID
    bleAdvertising->addServiceUUID(SERVICE_UUID);

    //don't know what these do, but necessary for the program to work
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x06); // Functions that help with iPhone connection issues
    // bleAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("characteristic defined...you can connect with your phone!");
}

// Put your main code here, to run repeatedly
void loop()
{
    if (deviceConnected) {
        // 1 - Update the characteristic's value (which can  be read by a client)

        //now when we read characteristic it will show the timer value

        // timer++;
        // bleCharacteristic->setValue(timer);
        // Serial.printf("%d written to BLE characteristic.\n", timer);

        // 2 - Read the characteristic's value as a string (which can be written from a client)

        // std::string readValue = bleCharacteristic->getValue();
        // Serial.printf("The new characteristic value as a STRING is: %s\n", readValue.c_str());
        // String valStr = readValue.c_str();
        // int val = valStr.toInt();
        // Serial.printf("The new characteristic value as an INT is: %d\n", val);

        // 3 - Read the characteristic's value as a BYTE (which can be written from a client)
        // initialize a pointer and get the characteristic value as a byte
        uint8_t *numPtr = new uint8_t();
        numPtr = bleCharacteristic->getData();
        Serial.printf("The new characteristic value as a BYTE is: %d\n", *numPtr);
    } else
        timer = 0;
    
    // Only update the timer (if connected) every 1 second
    delay(1000);
}