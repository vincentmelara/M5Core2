#include <Arduino.h>
#include <M5Core2.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // Initialize the device so it is responsive
  M5.begin();

  //Do some intitial stuff
  Serial.println("Hello From M5Stack Core2");
  M5.Lcd.setTextSize(3);
  M5.Lcd.fillScreen(BLUE);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.println("Hello World! Dr. Dan is the best. ");
}

void loop() {
  // Do some stuff repeatedly
  Serial.println("hello again!");
  delay(1000);
}