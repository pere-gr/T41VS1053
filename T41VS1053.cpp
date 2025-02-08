/*************************************************** 
  This is a library to use the Adafruit's VS1053 with Teensy4.1 (maybe others VS1053. Not tested!!)

  Based on:
    - Adafruit's https://github.com/adafruit/Adafruit_VS1053_Library
    - TobiasVanDyk's https://github.com/TobiasVanDyk/VS1053B-Teensy36-Teensy41-SDCard-Music-Player

  Hardware tested:
    - Teensy4.1 https://www.pjrc.com/store/teensy41.html
    - Adafruit's "Music Maker" MP3 Shield for Arduino (MP3/Ogg/WAV...) https://www.adafruit.com/product/1790

  PereGR - Feb 2025

 ****************************************************/

#define VS1053_CONTROL_SPI_SETTING SPISettings(250000, MSBFIRST, SPI_MODE0)  // 2.5 MHz SPI speed Control
#define VS1053_DATA_SPI_SETTING SPISettings(8000000, MSBFIRST, SPI_MODE0)    // 8 MHz SPI speed Data


#include "T41VS1053.h"


//Just to be able to use feedBuffer in an interrupt
static T41VS1053 *myself;
volatile boolean feedBufferLock = false;  //!< Locks feeding the buffer
static void feeder(void) { myself->feedBuffer(); }

uint8_t T41VS1053::begin(void) {
  uint8_t isInit;
  if (XRST >= 0) {
    pinMode(XRST, OUTPUT);  // if reset = -1 ignore
    digitalWrite(XRST, LOW);
  }

  pinMode(XCS, OUTPUT);
  digitalWrite(XCS, HIGH);
  pinMode(XDCS, OUTPUT);
  digitalWrite(XDCS, HIGH);


  SPI.begin();
  /*SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV128);*/

  reset();
  isInit = (sciRead(VS1053_REG_STATUS) >> 4) & 0x0F;

  interrupts();
  return isInit;
}

uint16_t T41VS1053::decodeTime(void) {
  cli();
  uint16_t t = sciRead(VS1053_REG_DECODETIME);
  sei();
  return t;
}

//////////////////////////////////////////////////////////
// Set the card to be disabled while we get the vs1053 up
//////////////////////////////////////////////////////////
void T41VS1053::disableCard(void) {
  playingMusic = false;
  pinMode(SDCS, OUTPUT);
  digitalWrite(SDCS, HIGH);
}

///////////////////////////////////////////////////
// XDREQ is low while the receive buffer is full
///////////////////////////////////////////////////
void T41VS1053::feedBuffer(void) {
  noInterrupts();
  // dont run twice in case interrupts collided
  // This isn't a perfect lock as it may lose one feedBuffer request if
  // an interrupt occurs before feedBufferLock is reset to false. This
  // may cause a glitch in the audio but at least it will not corrupt
  // state.
  if (feedBufferLock) {
    interrupts();
    return;
  }
  feedBufferLock = true;
  interrupts();

  feedBuffer_noLock();

  feedBufferLock = false;
}

void T41VS1053::feedBuffer_noLock(void) {
  if ((!playingMusic)  // paused or stopped
      || (!currentTrack) || (!isReadyForData())) {
    return;  // paused or stopped
  }
  // Feed the hungry buffer! :)
  while (isReadyForData()) {
    // Read some audio data from the SD card file
    int bytesread = currentTrack.read(SoundBuffer, VS1053_DATABUFFERLEN);
    if (bytesread == 0) {
      playingMusic = false;
      currentTrack.close();
      break;
    }
    playData(SoundBuffer, bytesread);
  }
}

uint16_t T41VS1053::getPlaySpeed(void) {
  noInterrupts();
  sciWrite(VS1053_SCI_WRAMADDR, VS1053_PARA_PLAYSPEED);
  uint16_t speed = sciRead(VS1053_SCI_WRAM);
  interrupts();
  return speed;
}

char *T41VS1053::getTrackName(void){
  return (char*)currentTrack.name();
}

boolean T41VS1053::isPaused(void) {
  return (!playingMusic && currentTrack);
}

boolean T41VS1053::isPlaying(void) {
  return !isStopped();
}

////////////////////////////////////////////////////
// XDREQ is true while the receive buffer has space
////////////////////////////////////////////////////
boolean T41VS1053::isReadyForData(void) {
  return digitalRead(XDREQ);
}

boolean T41VS1053::isStopped(void) {
  return (!playingMusic && !currentTrack);
}

void T41VS1053::pause(boolean pause) {
  if (pause) playingMusic = false;
  else {
    playingMusic = true;
    feedBuffer();
  }
}

// Play a file 
boolean T41VS1053::play(File& trackFile) {
  if (isPlaying()) stop();
  currentTrack = trackFile;
  if (!currentTrack) { return false; }

  if (hasInt) {
    Serial.println(F("Play in background..."));
    return playBackground();
  }
  Serial.println(F("Play and wait it ends"));
  return playFullFile();
}

// Opens and Play a file 
boolean T41VS1053::play(const char *trackName) {
  if (isPlaying()) stop();

  currentTrack = SD.open(trackName);
  if (!currentTrack) { return false; }

  if (hasInt) {
    Serial.println(F("Play in background..."));
    return playBackground();
  }
  Serial.println(F("Play and wait it ends"));
  return playFullFile();
}

boolean T41VS1053::playBackground(void) {
  //currentTrack = SD.open(trackname);
  if (!currentTrack) { return false; }
  playingMusic = true;
  while (!isReadyForData())
    ;                                                     // wait for ready for data
  while (playingMusic && isReadyForData()) feedBuffer();  // then send data
  return true;
}

void T41VS1053::playData(uint8_t *buffer, uint8_t buffsiz) {
  SPI.beginTransaction(VS1053_DATA_SPI_SETTING);
  digitalWrite(XDCS, LOW);

  for (uint8_t i = 0; i < buffsiz; i++) { spiWrite(buffer[i]); }  // buffsiz = 32

  digitalWrite(XDCS, HIGH);
  SPI.endTransaction();
}

boolean T41VS1053::playFullFile(void) {
  if (!playBackground()) return false;
  while (playingMusic) { feedBuffer(); }
  return true;
}

void T41VS1053::reset(void) {
  if (XRST >= 0) {
    digitalWrite(XRST, LOW);
    delay(100);
    digitalWrite(XRST, HIGH);
  }
  digitalWrite(XCS, HIGH);
  digitalWrite(XDCS, HIGH);
  delay(100);
  softReset();
  delay(100);

  sciWrite(VS1053_REG_CLOCKF, 0x6000);

  setVolume(40, 40);
}

uint16_t T41VS1053::sciRead(uint8_t addr) {
  uint16_t data;
  SPI.beginTransaction(VS1053_CONTROL_SPI_SETTING);
  digitalWrite(XCS, LOW);
  spiWrite(VS1053_SCI_READ);
  spiWrite(addr);
  delayMicroseconds(10);
  data = spiRead();
  data <<= 8;
  data |= spiRead();
  digitalWrite(XCS, HIGH);
  SPI.endTransaction();
  return data;
}

void T41VS1053::sciWrite(uint8_t addr, uint16_t data) {
  SPI.beginTransaction(VS1053_CONTROL_SPI_SETTING);
  digitalWrite(XCS, LOW);
  spiWrite(VS1053_SCI_WRITE);
  spiWrite(addr);
  spiWrite(data >> 8);
  spiWrite(data & 0xFF);
  digitalWrite(XCS, HIGH);
  SPI.endTransaction();
}

void T41VS1053::setPlaySpeed(uint16_t speed) {
  if (speed < 1 || speed > 4) speed = 1;
  noInterrupts();
  sciWrite(VS1053_SCI_WRAMADDR, VS1053_PARA_PLAYSPEED);
  sciWrite(VS1053_SCI_WRAM, speed);
  interrupts();
}

void T41VS1053::setVolume(uint8_t vol) {
  setVolume(vol,vol);
}

void T41VS1053::setVolume(uint8_t left, uint8_t right) {
  uint16_t v;
  v = left;
  v <<= 8;
  v |= right;

  cli();
  sciWrite(VS1053_REG_VOLUME, v);
  sei();
}

void T41VS1053::softReset(void) {
  sciWrite(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_RESET);
  delay(100);
}

uint8_t T41VS1053::spiRead(void) {
  int8_t x;
  x = 0;
  //clkportreg = portOutputRegister(digitalPinToPort(SCLK));
  //clkpin = digitalPinToBitMask(SCLK);
  // MSB first, clock low when inactive (CPOL 0), data valid on leading edge (CPHA 0)
  // Make sure clock starts low
  x = SPI.transfer(0x00);
  // Make sure clock ends low
  //*clkportreg &= ~clkpin;

  return x;
}

void T41VS1053::spiWrite(uint8_t c) {  // MSB first, clock low when inactive (CPOL 0), data valid on leading edge (CPHA 0)
  // Make sure clock starts low
  //clkportreg = portOutputRegister(digitalPinToPort(SCLK));
  //clkpin = digitalPinToBitMask(SCLK);
  SPI.transfer(c);
  //*clkportreg &= ~clkpin;   // Make sure clock ends low
}

void T41VS1053::stop(void) {
  playingMusic = false;
  currentTrack.close();
}

////////////////////////////////////////////////////////////////
// Configure interrupt for Data XDREQ from vs1053
// XDREQ is low while the receive buffer is full
////////////////////////////////////////////////////////////////
void T41VS1053::useInterrupt(void) {
  myself = this;

  pinMode(XDREQ, INPUT);
  SPI.usingInterrupt(XDREQ);  // Disable Interrupt during SPI transactions
  attachInterrupt(XDREQ, feeder, CHANGE);  // Interrupt on Pin XDREQ state change
  hasInt = true;
  return;
}
