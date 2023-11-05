#include <DFRobot_DF1101S.h>
#include <SoftwareSerial.h>
#include <MPR121.h>
#include <Wire.h>
#include <SdFat.h>
#include <FastLED.h>

SoftwareSerial df1101sSerial(2,3);
DFRobot_DF1101S df1101s;

SdFat sd;
SdFile myFile;
int recordCount = -1; // Number of recordings saved
const int CS_PIN = 10; // CS pin on SD card reader

const uint8_t NUM_LEDS = 20; // Number of LEDs
CRGB leds[NUM_LEDS]; // LED array

String recFileName; // Filename of last recording

const int MAX_RECORD_TIME = 10000; // Time limit of recordings (default: 10s)
long recordingStartTime = -1; // Timer variable to detect how long a recording is
bool halfwayTrigger = false; // Controls the colour change halfway through the recording period

const long REMINDER_MAX = 93600000; // 26 hours
const long REMINDER_MIN = 72000000; // 20 hours
long reminderTime;

//const long REMINDER_INTERVAL = 86400000; // Period in which there will be a reminder (randomly)
const int MS_GRACE_PERIOD = 60000; // Accounts for CPU slowdowns, small grace period added to the reminder time
//long reminderTrigger = 0; // The random time after which there will be a reminder
bool reminderActive = false; // If true, reminder is blinking, if false, it's not blinking

// Gets recording count from SD card text file C.TXT and stores in in the recordCount variable
void getAndSetRecordCount() {
  if (myFile.open("C.TXT", O_RDWR | O_CREAT)) {
    char buf[10];
    char c;
    uint8_t i = 0;
    while (myFile.available()) {
      if (i >= 10) break;
      c = myFile.read();
      buf[i] = c;
      i++;
    }
    int count = atoi(buf);
    Serial.print(F("read count from file: ")); Serial.println(count);
    if (count >= 0) recordCount = count;
    else recordCount = 0;
  }
  myFile.close();
}

// Saves recording count to C.TXT on the SD card
void saveRecordCount() {
  char countStr[10];
  itoa(recordCount, countStr, 10);

  if (myFile.open("C.TXT", O_RDWR | O_CREAT | O_TRUNC)) {
    myFile.write(countStr);
    Serial.print(F("Saving count to file: ")); Serial.println(countStr);
  } else {
    Serial.println(F("Count file failure"));
  }
  myFile.close();
}

// Figures out how many recordings there are on the recorder based on the filename of the last recording
void syncRecordCount(String filename) {
  if (filename.equalsIgnoreCase("error") || filename == "" || filename.length() == 0) {
    Serial.print(F("No filename, so won't sync count. Current recordCount is ")); Serial.println(recordCount);
    return;
  }
  String substr = filename.substring(13, 17);
  recordCount = substr.toInt() + 1;
  Serial.print(F("Syncing record count to: ")); Serial.println(recordCount);
  saveRecordCount();
}

// Saves a system log to LOG.TXT on the SD card
void saveSystemLog(char text[]) {
  char logStr[40];
  ultoa(millis(), logStr, 10);
  strcat(logStr, ": ");
  strcat(logStr, text);
  
  if (myFile.open("LOG.TXT", O_RDWR | O_CREAT | O_AT_END)) {
    myFile.println(logStr);
    Serial.print(F("Printed \"")); Serial.print(logStr); Serial.println(F("\" to log file"));
  } else {
    Serial.println(F("Log file failure"));
    sd.errorPrint(&Serial);
  }
  myFile.close();
}

// Change LED colours
void changeColour(CRGB c) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = c;
  }
  FastLED.show();
}

void setup() {
  Serial.begin(9600);

  randomSeed(analogRead(0));
  
  df1101sSerial.begin(115200);
  if (!df1101s.begin(df1101sSerial)) {
    Serial.println(F("Recorder init failed"));
  } else {
    // Make sure recorder lady isn't saying stuff
    df1101s.setPrompt(false);
    df1101s.setVol(50);
  }

  if (!MPR121.begin(0x5A)) { 
    Serial.println(F("MPR121 init failed"));
  } else {
    MPR121.setInterruptPin(4);
    MPR121.setTouchThreshold(20);
    MPR121.setReleaseThreshold(10);
    MPR121.setTouchThreshold(6, 10);
    MPR121.setReleaseThreshold(6, 5);
  }

  FastLED.addLeds<NEOPIXEL, 9>(leds, NUM_LEDS);
  //FastLED.setBrightness(150);
  changeColour(CRGB::White);

  if (!sd.begin(CS_PIN, SPI_HALF_SPEED)) { 
    Serial.println(F("SD init failed"));
  } else {
    // If SD card inits, log that setup is complete and fetch recording count
    saveSystemLog("setup complete");
    getAndSetRecordCount();
  }

  // Set first trigger time for reminders
  reminderTime = millis() + random(REMINDER_MIN, REMINDER_MAX);
  Serial.print(F("Next reminder in ")); Serial.print(reminderTime - millis()); Serial.println(F(" ms"));
//  reminderTrigger = random(REMINDER_INTERVAL) + millis();
}

// Stop a recording that is in progress
// The "forced" argument just tells us what kind of system log to save.
// If "forced" = true, it means times up on a recording. If false, it means "stop" was pressed.
void stopRecording(bool forced) {
  if (recordingStartTime == -1) return;
  recordingStartTime = -1;
  Serial.println(F("Stop recording"));
  
  if (!forced) saveSystemLog("touch stop rec");
  else saveSystemLog("times up, stop rec");

  recFileName = df1101s.saveRec();
  Serial.print(F("Save recording as: ")); Serial.println(recFileName);
  df1101s.switchFunction(df1101s.MUSIC);
  syncRecordCount(recFileName);
  changeColour(CRGB::White);
  halfwayTrigger = false; 
}

// Start a recording
void startRecording() {
  halfwayTrigger = false;

  // Remove reminder and set a new time for the next one
  if (reminderActive == true) {
//    reminderTrigger = random(REMINDER_INTERVAL);
    reminderActive = false;
    reminderTime = millis() + random(REMINDER_MIN, REMINDER_MAX);
    Serial.print(F("Next reminder in ")); Serial.print(reminderTime - millis()); Serial.println(F(" ms"));
  }
  
  recordingStartTime = millis();
  saveSystemLog("touch record");
  Serial.println(F("Recording..."));
  changeColour(CRGB::Orange);
  df1101s.switchFunction(df1101s.RECORD);
  df1101s.start();
}

// Start playback of a random recording
void startPlayback() {
  recordingStartTime = -1;
  halfwayTrigger = false;

  for (uint8_t i = 0; i < 4; i++) {
    changeColour(CRGB::Green);
    delay(100);
    changeColour(CRGB::Black);
    delay(100);
  }

  changeColour(CRGB::White);
  
  Serial.println(F("Play recording"));
  saveSystemLog("touch playback");
  df1101s.switchFunction(df1101s.MUSIC);
  df1101s.setPlayMode(df1101s.SINGLE);
  int randomFile = random(0, recordCount + 1);
  Serial.print(F("Playing random file: ")); Serial.println(randomFile);
  df1101s.playSpecFile(randomFile);
}

void loop() {

  // Activate reminder if there isn't one already active and if it's time to trigger it
  //if (millis() - reminderTrigger <= MS_GRACE_PERIOD && !reminderActive && millis() > reminderTrigger && recordingStartTime <= 0) {
  if (millis() > reminderTime && !reminderActive) {
    Serial.println(F("Activating reminder"));
    saveSystemLog("reminder on");
    reminderActive = true;
  }

  // Blink blue if reminder is active
  if (reminderActive) {
    changeColour(CRGB::Blue);
    delay(100);
    changeColour(CRGB::Black);
    delay(100);
  }

  // If we are actively recording...
  if (recordingStartTime > 0) {
    // Check if max time is up (default is 10s)
    if ((millis() - recordingStartTime) >= MAX_RECORD_TIME) {
      Serial.println(F("times up!"));
      stopRecording(true);
    // If recording time is halfway done then change the colour to red
    } else if ((millis() - recordingStartTime) >= (MAX_RECORD_TIME / 2) && !halfwayTrigger) {
      Serial.println(F("Halfway to time limit"));
      changeColour(CRGB::Red);
      halfwayTrigger = true;
    }
  }

  // Touch point stuff
  MPR121.updateAll();
  if (MPR121.getNumTouches() == 1) {
    if (MPR121.isNewTouch(0)) {
      startRecording();
    } else if (MPR121.isNewTouch(6)) {
      // stopRecording(false);
    } else if (MPR121.isNewTouch(10)) {
      startPlayback();
    }
  }
}
