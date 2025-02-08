#include "pgr_teensy41_vs1053.h"

T41VS1053 vs;

// the setup routine runs once when you press reset:
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("------------------------- Setup -----------------------------");
  
  while (! vs.begin()) { // initialise the music player
    Serial.println(F("\t> Searching vs1053..."));
    delay(500);
  }
  Serial.println(F("VS1053 found"));

  SD.begin(SDCS);    // initialise the SD card

  // Set volume for left, right channels. lower numbers is higher volume
  vs.setVolume(30, 30);
  vs.playFullFile("file.mp3");
}

// the loop routine runs over and over again forever:
void loop() {
  Serial.println("------------------------- Loop -----------------------------");
  
}