/*
  Play-All-MP3-Files-on-SD

  This example plays all mp3-files from microSD card. Output goes to speaker and headphone 
  sockets.

  The "SD" library requires the cards CS signal (GPIO10). Therefore solder bridge SD_CS must 
  be closed [default].

  The TLV320 initialization sequence is based on Adafruit_TLV320_I2S lib examples and
  has been modified to fit the TLV320DAC3101 Stereo Audio DAC on the YB-ESP32-S3-DAC board.

  The following libraries are needed:
   - SD
   - ESP32-audioI2S
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-02-25, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "Audio.h"
#include "TLV320DAC3101.h"

#define MAX_PATH_DEPTH 4

TLV320DAC3101 dac;
Audio audio;
SPIClass *spi_onboardSD = new SPIClass(FSPI);
File root, entry;
bool playing = false;
std::vector<File> dirChain;

void halt(const char *message) {
  Serial.println(message);
  while (true) yield(); // Function to halt on critical errors
}

void searchAudioFiles() {
  while (!playing && dirChain.size()) {
    entry = dirChain[dirChain.size() - 1].openNextFile();
    if (!entry) {
      // no (more) files in this directory
      dirChain[dirChain.size() - 1].close();
      dirChain.pop_back();
      break;
    }
    if (entry.isDirectory()) {
      if (dirChain.size() < MAX_PATH_DEPTH) {
        dirChain.push_back(entry);  // dir entry stays open while member of chain
        break;
      }
    }
    else if (String(entry.name()).endsWith("mp3")) {
      Serial.print("now playing: ");
      Serial.println(String(entry.path()));
      audio.connecttoFS(SD, entry.path());
      entry.close();
      playing = true;
      break;
    }
    entry.close();
  }
}

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
    if ( m.e == Audio::evt_eof ) playing = false;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.println("running example \"Play-All-MP3-Files-on-SD\":");

  spi_onboardSD->begin(SCK, MISO, MOSI, SS);

  if (!SD.begin(SS, *spi_onboardSD)) {
    Serial.println("error mounting microSD");
    return;
  }
  digitalWrite(LED_BUILTIN, HIGH); // status LED On
  Serial.println("Mounting microSD ok");

  root = SD.open("/");
  dirChain.push_back(root);

  // TLV320DAC3101 Audio DAC initialization
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);    // resets the DAC chip
  delay(100);
  digitalWrite(TLV_RESET, HIGH);

  Serial.println("Init TLV320 DAC");
  if (!dac.begin()) {
    halt("Failed to initialize codec!");
  }
  delay(10);

  // I2S Interface Control
  if (!dac.setCodecInterface(TLV320DAC3100_FORMAT_I2S,       // Format: I2S (Philips standard)
                             TLV320DAC3100_DATA_LEN_16)) {   // Length: 16 bits
    halt("Failed to configure codec interface!");
  }

  // Clock MUX and PLL settings
  if (!dac.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL) || // PLL output feeds Codec
      !dac.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK)) {    // BCLK feeds PLL input
    halt("Failed to configure codec clocks!");
  }

  if (!dac.setPLLValues(1, 2, 32, 0)) {      // Configure PLL dividers P, R, J and D
    halt("Failed to configure PLL values!");
  }

  if (!dac.setNDAC(true, 8) ||               // Configure DAC dividers NDAC, MDAC and DOSR
      !dac.setMDAC(true, 2) ||
      !dac.setDOSR(128)) {
    Serial.println("Failed to configure DAC dividers!");
  }

  if (!dac.powerPLL(true)) { // Power up the PLL
    halt("Failed to power up PLL!");
  }

  // DAC Setup
  if (!dac.setDACDataPath(true, true,                      // Power up both DACs
                          TLV320_DAC_PATH_NORMAL,          // Normal left path
                          TLV320_DAC_PATH_NORMAL,          // Normal right path
                          TLV320_VOLUME_STEP_1SAMPLE)) {   // Step: 1 per sample
    halt("Failed to configure DAC data path!");
  }

  if (!dac.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER,   // Left DAC to mixer
                                 TLV320_DAC_ROUTE_MIXER,   // Right DAC to mixer
                                 false, false, false,      // No AIN routing
                                 false)) {                 // No HPL->HPR
    halt("Failed to configure DAC routing!");
  }

  // DAC Volume Control
  if (!dac.setDACVolumeControl(
        false, false, TLV320_VOL_INDEPENDENT) ||   // Unmute both channels
      !dac.setChannelVolume(false, 10) ||          // Left DAC +10dB
      !dac.setChannelVolume(true, 10)) {           // Right DAC +10dB
    halt("Failed to configure DAC volumes!");
  }

  // Headphone and Speaker Setup
  if (!dac.configureHeadphoneDriver(
        true, true,                           // Power up both drivers
        TLV320_HP_COMMON_1_65V,               // Default common mode
        false) ||                             // Don't power down on SCD
      !dac.configureHPL_PGA(0, true) ||       // Set HPL gain, unmute
      !dac.configureHPR_PGA(0, true) ||       // Set HPR gain, unmute
      !dac.setHPLVolume(true, 20) ||          // Enable and set HPL volume to -10dB
      !dac.setHPRVolume(true, 20)) {          // Enable and set HPR volume to -10dB
    halt("Failed to configure headphone outputs!");
  }

  if (!dac.enableSpeaker(true) ||                 // Disable/Enable speaker amps
      !dac.configureSPK_PGA(TLV320_SPK_GAIN_6DB,  // Set gain to 6dB
                            true) ||              // Unmute
      !dac.setSPKVolume(true, 20)) {              // Enable and set volume to -10dB
    halt("Failed to configure speaker output!");
  }

  Serial.println("TLV320 DAC config done!");

  audio.audio_info_callback = my_audio_info;
  audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  audio.setVolume(10); // 0...21(max)
}

void loop() {
  if (!playing) {
    searchAudioFiles();
  }
  audio.loop();  // play mp3 audio file
  vTaskDelay(1); // needed by ESP32-audioI2S lib!
}
