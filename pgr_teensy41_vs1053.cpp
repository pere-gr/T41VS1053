

#define VS1053_CONTROL_SPI_SETTING SPISettings(250000, MSBFIRST, SPI_MODE0)  // 2.5 MHz SPI speed Control
#define VS1053_DATA_SPI_SETTING SPISettings(8000000, MSBFIRST, SPI_MODE0)    // 8 MHz SPI speed Data


#include "pgr_teensy41_vs1053.h"


//Just to be able to use feedBuffer in an interrupt
static T41VS1053 *myself;
static void feeder(void) {
  myself->feedBuffer();
}

////////////////////////////////////////////////////////////////
// Configure interrupt for Data XDREQ from vs1053
// XDREQ is low while the receive buffer is full
////////////////////////////////////////////////////////////////
void T41VS1053::interrupt(void) {
  SPI.usingInterrupt(XDREQ);               // Disable Interrupt during SPI transactions
  attachInterrupt(XDREQ, feeder, CHANGE);  // Interrupt on Pin XDREQ state change
}  // feeder->feedBuffer executed (lines 26, 209)

//////////////////////////////////////////////////////////
// Set the card to be disabled while we get the vs1053 up
//////////////////////////////////////////////////////////
void T41VS1053::disableCard(void) {
  playingMusic = false;
  pinMode(SDCS, OUTPUT);
  digitalWrite(SDCS, HIGH);
}

/////////////////////////////////////////////////////////////////////////
// Play file without interrupts
/////////////////////////////////////////////////////////////////////////
boolean T41VS1053::playFullFile(const char *trackname) {
  if (!startPlayingFile(trackname)) return false;
  while (playingMusic) { feedBuffer(); }
  return true;
}

/////////////////////////
void T41VS1053::stop(void) {
  playingMusic = false;
  currentTrack.close();
}

///////////////////////////////////////
void T41VS1053::pause(boolean pause) {
  if (pause) playingMusic = false;
  else {
    playingMusic = true;
    feedBuffer();
  }
}

///////////////////////
boolean T41VS1053::isPaused(void) {
  return (!playingMusic && currentTrack);
}

////////////////////////
boolean T41VS1053::isStopped(void) {
  return (!playingMusic && !currentTrack);
}

//////////////////////////////////////////////////////
boolean T41VS1053::startPlayingFile(const char *trackname) {
  currentTrack = SD.open(trackname);
  if (!currentTrack) { return false; }
  playingMusic = true;
  while (!isReadyForData())
    ;                                                     // wait for ready for data
  while (playingMusic && isReadyForData()) feedBuffer();  // then send data
  return true;
}

///////////////////////////////////////////////////
// XDREQ is low while the receive buffer is full
///////////////////////////////////////////////////
void T41VS1053::feedBuffer(void) {
  myself = this;  // oy vey

  static uint8_t running = 0;
  uint8_t sregsave;

  // Do not allow 2 FeedBuffer instances to run concurrently
  noInterrupts();
  // paused or stopped. no SDCard track open, XDREQ=0 receive buffer full
  if ((!playingMusic) || (!currentTrack) || (!isReadyForData())) {
    running = 0;
    return;
  }

  interrupts();

  // Send buffer
  while (isReadyForData()) {
    int bytesread = currentTrack.read(SoundBuffer, VS1053_DATABUFFERLEN);
    if (bytesread == 0)  // End of File
    {
      playingMusic = false;
      currentTrack.close();
      running = 0;
      return;
    }
    playData(SoundBuffer, bytesread);
  }
  running = 0;
  return;
}

////////////////////////////////////////////////////
// XDREQ is true while the receive buffer has space
////////////////////////////////////////////////////
boolean T41VS1053::isReadyForData(void) {
  return digitalRead(XDREQ);
}

//////////////////////////////////////////////////////
void T41VS1053::playData(uint8_t *buffer, uint8_t buffsiz) {
  SPI.beginTransaction(VS1053_DATA_SPI_SETTING);
  digitalWrite(XDCS, LOW);

  for (uint8_t i = 0; i < buffsiz; i++) { spiWrite(buffer[i]); }  // buffsiz = 32

  digitalWrite(XDCS, HIGH);
  SPI.endTransaction();
}

//////////////////////////////////////////////////
void T41VS1053::setVolume(uint8_t left, uint8_t right) {
  uint16_t v;
  v = left;
  v <<= 8;
  v |= right;

  cli();
  sciWrite(VS1053_REG_VOLUME, v);
  sei();
}

////////////////////////////
uint16_t T41VS1053::decodeTime(void) {
  cli();
  uint16_t t = sciRead(VS1053_REG_DECODETIME);
  sei();
  return t;
}

///////////////////////
void T41VS1053::softReset(void) {
  sciWrite(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_RESET);
  delay(100);
}

//////////////////////////
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

//////////////////////
uint8_t T41VS1053::begin(void) {
  if (XRST >= 0) {
    pinMode(XRST, OUTPUT);  // if reset = -1 ignore
    digitalWrite(XRST, LOW);
  }

  pinMode(XCS, OUTPUT);
  digitalWrite(XCS, HIGH);
  pinMode(XDCS, OUTPUT);
  digitalWrite(XDCS, HIGH);
  pinMode(XDREQ, INPUT);

  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV128);

  reset();

  return (sciRead(VS1053_REG_STATUS) >> 4) & 0x0F;
}

/////////////////////////////////////
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

//////////////////////////////////////////////
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

static volatile uint8_t *clkportreg;
static uint8_t clkpin;

////////////////////////
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

///////////////////////////////
void T41VS1053::spiWrite(uint8_t c) {  // MSB first, clock low when inactive (CPOL 0), data valid on leading edge (CPHA 0)
  // Make sure clock starts low
  //clkportreg = portOutputRegister(digitalPinToPort(SCLK));
  //clkpin = digitalPinToBitMask(SCLK);
  SPI.transfer(c);
  //*clkportreg &= ~clkpin;   // Make sure clock ends low
}