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
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// Connect SCLK, MISO and MOSI to standard hardware SPI pins.
#define SCLK 13       // SPI Clock shared with SD card
#define MISO 12       // Input data from vs1053 or SD card
#define MOSI 11       // Output data to vs1053 or SD card

// These are the pins used for the Adafruit vs1053B breakout module
#define XRST  -1 //8                // vs1053 reset (output)
#define XCS   10               // vs1053 chip select (output)
#define XDCS  5                // vs1053 Data select (output)
#define XDREQ 3                // vs1053 Data Ready an Interrupt pin (input)
#define SDCS  BUILTIN_SDCARD   // Use Teensy built-in card
// For Teensy 3.5, 3.6, 4.0, 4.1 better to use its built-in SDCard
//#define SDCS 4                // Use vs1053 SDCard Card chip select pin


#define VS1053_FILEPLAYER_TIMER0_INT 255  //!< Allows useInterrupt to accept pins 0 to 254
#define VS1053_FILEPLAYER_PIN_INT 3      //!< Allows useInterrupt to accept pins 0 to 254

#define VS1053_SCI_READ 0x03   //!< Serial read address
#define VS1053_SCI_WRITE 0x02  //!< Serial write address

#define VS1053_REG_MODE 0x00        //!< Mode control
#define VS1053_REG_STATUS 0x01      //!< Status of VS1053b
#define VS1053_REG_BASS 0x02        //!< Built-in bass/treble control
#define VS1053_REG_CLOCKF 0x03      //!< Clock frequency + multiplier
#define VS1053_REG_DECODETIME 0x04  //!< Decode time in seconds
#define VS1053_REG_AUDATA 0x05      //!< Misc. audio data
#define VS1053_REG_WRAM 0x06        //!< RAM write/read
#define VS1053_REG_WRAMADDR 0x07    //!< Base address for RAM write/read
#define VS1053_REG_HDAT0 0x08       //!< Stream header data 0
#define VS1053_REG_HDAT1 0x09       //!< Stream header data 1
#define VS1053_REG_VOLUME 0x0B      //!< Volume control

#define VS1053_GPIO_DDR 0xC017    //!< Direction
#define VS1053_GPIO_IDATA 0xC018  //!< Values read from pins
#define VS1053_GPIO_ODATA 0xC019  //!< Values set to the pins

#define VS1053_INT_ENABLE 0xC01A  //!< Interrupt enable

#define VS1053_MODE_SM_DIFF 0x0001      //!< Differential, 0: normal in-phase audio, 1: left channel inverted
#define VS1053_MODE_SM_LAYER12 0x0002   //!< Allow MPEG layers I & II
#define VS1053_MODE_SM_RESET 0x0004     //!< Soft reset
#define VS1053_MODE_SM_CANCEL 0x0008    //!< Cancel decoding current file
#define VS1053_MODE_SM_EARSPKLO 0x0010  //!< EarSpeaker low setting
#define VS1053_MODE_SM_TESTS 0x0020     //!< Allow SDI tests
#define VS1053_MODE_SM_STREAM 0x0040    //!< Stream mode
#define VS1053_MODE_SM_SDINEW 0x0800    //!< VS1002 native SPI modes
#define VS1053_MODE_SM_ADPCM 0x1000     //!< PCM/ADPCM recording active
#define VS1053_MODE_SM_LINE1 0x4000     //!< MIC/LINE1 selector, 0: MICP, 1: LINE1
#define VS1053_MODE_SM_CLKRANGE 0x8000  //!< Input clock range, 0: 12..13 MHz, 1: 24..26 MHz

#define VS1053_SCI_AIADDR 0x0A    //!< Indicates the start address of the application code written earlier
                                  //!< with SCI_WRAMADDR and SCI_WRAM registers.
#define VS1053_SCI_AICTRL0 0x0C   //!< SCI_AICTRL register 0. Used to access the user's application program
#define VS1053_SCI_AICTRL1 0x0D   //!< SCI_AICTRL register 1. Used to access the user's application program
#define VS1053_SCI_AICTRL2 0x0E   //!< SCI_AICTRL register 2. Used to access the user's application program
#define VS1053_SCI_AICTRL3 0x0F   //!< SCI_AICTRL register 3. Used to access the user's application program
#define VS1053_SCI_WRAM 0x06      //!< RAM write/read
#define VS1053_SCI_WRAMADDR 0x07  //!< Base address for RAM write/read

#define VS1053_PARA_PLAYSPEED 0x1E04  //!< 0,1 = normal speed, 2 = 2x, 3 = 3x etc

#define VS1053_DATABUFFERLEN 32  //!< Length of the data buffer

/*!
 * Driver for the Adafruit VS1053
 */
class T41VS1053 {
public:
  uint8_t begin(void);
  boolean isPaused(void);
  boolean isPlaying(void);
  boolean isStopped(void);
  void pause(boolean pause);
  boolean play(File trackFile);
  boolean play(const char *trackname);
  boolean playFullFile(void);
  boolean playBackground(void);
  void reset(void);
  void setVolume(uint8_t left, uint8_t right);
  void softReset(void);
  void stop(void);
  char *trackName();
  void useInterrupt(void);

  //////////////////////////////////////////////
  // You're not going to use it normally      //
  //////////////////////////////////////////////
  uint16_t decodeTime(void);
  void disableCard(void);
  void feedBuffer(void);
  void feedBuffer_noLock(void);
  boolean isReadyForData(void);
  void playData(uint8_t *buffer, uint8_t buffsiz);
  uint16_t sciRead(uint8_t addr);
  void sciWrite(uint8_t addr, uint16_t data);
  uint8_t spiRead(void);
  void spiWrite(uint8_t c);

  File currentTrack;
  boolean hasInt = false;
  boolean playingMusic;
  uint8_t SoundBuffer[VS1053_DATABUFFERLEN];
};