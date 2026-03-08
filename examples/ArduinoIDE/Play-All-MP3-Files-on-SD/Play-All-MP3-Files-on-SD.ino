/*
  Play-All-MP3-Files-on-SD

  This example plays all mp3-files from microSD card. Output goes to speaker and headphone sockets.

  "SD" library requires the cards CS signal (GPIO10). Therefore solder bridge SD_CS must
  be closed [default].

  The ESP32-audioI2S Lib predefines the sample frequency of 44100Hz.

  The following libraries are needed:
   - SD
   - ESP32-audioI2S v3.4.x
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-03-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "Audio.h"
#include "TLV320DAC3101.h"

#define MAX_PATH_DEPTH 4

TLV320DAC3101 dac;
tlv320_init_config_t cfg;
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

  // init SD card
  spi_onboardSD->begin(SCK, MISO, MOSI, SS);
  if (!SD.begin(SS, *spi_onboardSD)) {
    Serial.println("error mounting microSD");
    return;
  }
  digitalWrite(LED_BUILTIN, HIGH); // status LED On
  Serial.println("Mounting microSD ok");

  root = SD.open("/");
  dirChain.push_back(root);

  // HW reset makes sure DAC chip is reset properly
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(10);
  digitalWrite(TLV_RESET, HIGH);

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = 44100.0;  // Hz, must be defined
  cfg.dac_gain_left = 10.0;        // dB, defaults to 0dB when not set,
  cfg.dac_gain_right = 10.0;       // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg)) {
    halt(dac.getLastError().c_str());
  }

  // activate headphone output and set headphone volume
  if (!dac.configHeadphoneOutput(true,              // enable headphone output
                                 false,             // HP(L/R) output driver acts as headphone driver
                                 100)) {            // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure headphone output!");
  }

  // activate speaker output and set speaker volume
  if (!dac.configSpeakerOutput(true,              // enable speaker output
                               90)) {             // set volume (allowed range: 0(quiet)...127(loud))
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
