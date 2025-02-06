#include <Arduino.h>
#include <M5Core2.h>

/////////////////////////////////////////////
// CHALLENGE: 
//    MODE 1 (normal): GREEN=>YELLOW=>RED=>GREEN... (every 1s)
//    MODE 2 (emergency): RED=>BLACK=>RED (every 1s)
//    BTN_A toggles between modes 1 and 2; init to mode 1
/////////////////////////////////////////////


// ENUM DEFINITIONS
enum State {S_GREEN, S_YELLOW, S_RED, S_BLACK};
enum Mode {M_NORMAL, M_EMERGENCY};


// GLOBAL VARIABLES
static State state = S_GREEN;
static Mode mode = M_NORMAL;
static unsigned long lastTS = 0;
static bool stateChangedThisLoop = false;
static unsigned long stateChangedIntervalMillis = 1000;


// RUNS ONCE AT STARTUP
void setup() {

  // Initialize the device so it is responsive
  M5.begin();
  stateChangedThisLoop = true;

}

// RUNS REPEATEDLY AFTER setup() COMPLETES
void loop() {
  //---------------------STATE TRANSITIONS---------------------
  //process state transition based on button press (mode change)
  M5.update();
  if (M5.BtnA.wasPressed()){
    lastTS = millis();
    stateChangedThisLoop = true;

    if(mode == M_NORMAL){
      mode = M_EMERGENCY;
      state = S_BLACK;
    } else {
      mode = M_NORMAL;
      state = S_RED;
    }
  }

  //process state transition based on 1s time interval
  if(millis() >= lastTS + stateChangedIntervalMillis){
    // update the timestamp and set the state change flag
    lastTS = millis();
    stateChangedThisLoop = true;

    //update the state based on the current mode
    if(mode == M_NORMAL){
      if(state == S_GREEN){
        state = S_YELLOW;
      } else if (state == S_YELLOW || state == S_BLACK){
        state = S_RED;
      } else if (state == S_RED){
        state = S_GREEN;
      }
    } else { //mode == M_EMERGENCY
      if(state == S_BLACK){
        state = S_RED;
      } else if (state == S_RED || state == S_YELLOW || state == S_GREEN){
        state = S_BLACK;
      }
    }
  }




  //---------------------STATE EXECUTION/OUTPUT---------------------

  //Take action based on the current state (i.e., set the LCD screen color)
  if(stateChangedThisLoop){
    //apply the proper color for the current state
    if(state == S_GREEN){
      M5.Lcd.fillScreen(GREEN);
    } else if (state == S_YELLOW){
      M5.Lcd.fillScreen(YELLOW);
    } else if (state == S_RED){
      M5.Lcd.fillScreen(RED);
    } else if (state == S_BLACK){
      M5.Lcd.fillScreen(BLACK);
    }

    //reset the state change flag
    stateChangedThisLoop = false;
  }
}
