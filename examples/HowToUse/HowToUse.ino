/*

  Connect the Adafruit's VS1053 as it has to be connected and place a MP3 named file.mp3 in SD's root folder
  Pins are defined in the .h file

  The sketch is going to play the track until the end of the time (or you turn off your beloved Teensy4.1)

  ** It uses interrupt by default. Comment line#39 if you won't interrupt.

*/

#include "T41VS1053.h"

T41VS1053 vs; 

// the setup routine runs once when you press reset:
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("------------------------- Setup -----------------------------");

  while (!vs.begin()) {  // initialise the music player, it can take few secs
    Serial.println(F("\t> Searching vs1053..."));
    delay(500);
  }
  Serial.println(F("VS1053 found"));

  Serial.println(F("Initialise the SD card"));
  SD.begin(SDCS);  // initialise the SD card

  // Set volume for left, right channels. lower numbers is higher volume
  Serial.println(F("Set volume for left, right channels"));
  vs.setVolume(50, 50);

  Serial.println(F("useInterrupt"));
  vs.useInterrupt(); // Comment to do not use interrupt
}

// the loop routine runs over and over again forever:
void loop() {
  if (!vs.isStopped()) { 
    Serial.print(F("Playing... "));
    Serial.println(vs.trackName());
  } else {
    Serial.println(F("Nothing playing"));
    //Let's play the file ... and again, and again, and again...
    vs.play("file.mp3"); // When useInterrupt, plays in background. Otherwise, play the full file then stops
  }

  delay(5000);
}