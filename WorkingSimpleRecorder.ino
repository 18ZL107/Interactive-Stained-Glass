#include <DFRobot_DF1101S.h>
#include <SoftwareSerial.h>
#include <MPR121.h>
#include <Wire.h>
#include <SD.h>
#include "FastLED.h"

SoftwareSerial df1101sSerial(2, 3);  //TX  RX
DFRobot_DF1101S df1101s;

String RECFileName;  //Recording file name 

File countFile;
int recordCount = 0;

const uint8_t NUM_LEDS = 2;
CRGB leds[NUM_LEDS];

void changeColour(CRGB c) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = c;
  }
  FastLED.show();
}

void saveRecordCount() {
  if (SD.exists("COUNT.TXT") {
    SD.remove("COUNT.TXT");
  }
  countFile = SD.open("COUNT.TXT", FILE_WRITE);
  if (countFile) {
    Serial.print("Saving record count: ");
    Serial.println(recordCount);
    countFile.println(recordCount);
    countFile.close();
  } else {
    Serial.println("Error saving record count.");
  }
}

void setup(void){
  Serial.begin(115200);
  df1101sSerial.begin(115200);
  while(!df1101s.begin(df1101sSerial)){
    Serial.println("Init failed, please check the wire connection!");
    delay(1000);
  }
  df1101s.setPrompt(false);
  df1101s.setVol(20);

  if (!MPR121.begin(0x5A)) Serial.println(F("Touch failed"));
  MPR121.setInterruptPin(4);
  MPR121.setTouchThreshold(40);
  MPR121.setReleaseThreshold(35);

  pinMode(10, OUTPUT);
  if (!SD.begin(10)) Serial.println("SD failed!");

  countFile = SD.open("COUNT.TXT", FILE_WRITE);
  if (countFile) {
    Serial.print("Recording count: ");
    recordCount = countFile.parseInt();
    Serial.println(count);
    countFile.close();
  } else {
    Serial.println("Error opening file record count.");
  }

  FastLED.addLeds<NEOPIXEL, 9>(leds, NUM_LEDS);
  changeColour(CRGB::White);
}

void loop(){
  MPR121.updateAll();
  if (MPR121.getNumTouches() == 1) {
    if (MPR121.isNewTouch(0)) {
      df1101s.switchFunction(df1101s.RECORD);
      df1101s.start();
      changeColour(CRGB::Red);
    } else if (MPR121.isNewTouch(6)) {
      delay(500);
      RECFileName = df1101s.saveRec();
      Serial.println(RECFileName);
      changeColour(CRGB::White);
      df1101s.switchFunction(df1101s.MUSIC);
      recordCount++;
      saveRecordCount();
    }
  }
}
