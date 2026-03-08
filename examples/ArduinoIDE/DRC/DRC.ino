/*
  Play Audio from SD with DRC activated/deactivated

  This example plays all mp3-files from an attached SD card. Output goes to speaker 
  and headphone sockets. DRC (Dynamic Range Compression) is set and enabled.

  The example accepts the following serial input:
   - d...disable DRC,
   - e...enable DRC,
   - +/-...increase/decrease volume
   - a...toggle adaptive mode

  The following libraries are needed:
   - SD
   - ESP32-audioI2S
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

#define MAX_PATH_DEPTH 4      // directory search depth on SD card
#define SAMPLERATE_HZ  44100  // predefined by ESP32-audioI2S library

TLV320DAC3101 dac;
tlv320_drc_param_t drc;
tlv320_init_config_t cfg;
Audio audio;
SPIClass *spi_onboardSD = new SPIClass(FSPI);
File root, entry;
std::vector<File> dirChain;
bool playing = false;
float channelVol = 10.0;      // dB, volume setting to start with

// non-standard DRC LPF/HPF coefficients as used in example in Ch. 6.3.10.4.6
uint8_t my_drc_lpf_coeffs[6] = {0x00, 0x11, 0x00, 0x11, 0x7F, 0xDE};
uint8_t my_drc_hpf_coeffs[6] = {0x7F, 0xAB, 0x80, 0x55, 0x7F, 0x56};

void halt(const char *message) {
  Serial.println(message);
  while (true) yield();  // Function to halt on critical errors
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
  Serial.println("\nrunning example \"Play Audio from microSD with DRC activated/deactivated\":");

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
  cfg.sample_frequency = SAMPLERATE_HZ;      // Hz, must be set
  cfg.dac_gain_left = channelVol;            // dB, defaults to 0dB when not set,
  cfg.dac_gain_right = channelVol;           // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg, false)) {           // set registers but keep DACs powered down
    halt(dac.getLastError().c_str());
  }

  // PRB_P2 (RC12) contains DRC filtering option
  if (!dac.setDACProcessingBlock(2)) {
    halt("Failed to configure Processing Block!");
  }

  // set & enable DRC with some non-standard parameters
  drc.hyst = TLV320_DRC_HYST_2DB;
  drc.lpf_coeffs = my_drc_lpf_coeffs;
  drc.hpf_coeffs = my_drc_hpf_coeffs;

  if (!dac.setDRC(true,                // enable DRC
                  true,                // on left channel and
                  true,                // on right channel
                  &drc)) {             // NULL for standard parameters
    halt("Failed to configure DRC!");
  }

  // power up both DACs
  if (!dac.powerOnDAC(true, true)) {
    halt("Failed to power on DACs!");
  }

  // activate headphone output and set headphone volume
  if (!dac.configHeadphoneOutput(true,              // enable headphone output
                                 false,             // HP(L/R) output driver acts as headphone driver
                                 70)) {             // set volume (allowed range: 0(quiet)...127(loud))
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

  // adaptive mode gets enabled with I2S bus active and DACs powered up
  dac.setAdaptiveMode(true);
}

void loop() {
  if (!playing) {
    searchAudioFiles();
  }
  audio.loop();   // play mp3 audio file
  vTaskDelay(1);  // needed by ESP32-audioI2S lib!

  char buf[10];
  if (Serial.read(buf, sizeof(buf))) {
    if (*buf == 'e') {
      // enable DRC
      if (!dac.setDRC(true, true, true, &drc)) {
        halt("Failed to enable DRC!");
      }
      Serial.println("DRC enabled");
    }
    else if (*buf == 'd') {
      // disable DRC
      if (!dac.setDRC(false, true, true, NULL)) {
        halt("Failed to disable DRC!");
      }
      Serial.println("DRC disabled");
    }
    else if (*buf == '+') {
      // increase volume
      channelVol += 1.0;
      if (channelVol > 24.0) channelVol = 24.0;
      if (!dac.setChannelVolume(false, channelVol) ||           // Left DAC
          !dac.setChannelVolume(true, channelVol)) {            // Right DAC
        Serial.println("Failed to configure DAC volumes!");
      }
      Serial.printf("DAC volume now %.1f\n", channelVol);
    }
    else if (*buf == '-') {
      // decrease volume
      channelVol -= 1.0;
      if (channelVol < -63.0) channelVol = -63.0;
      if (!dac.setChannelVolume(false, channelVol) ||           // Left DAC
          !dac.setChannelVolume(true, channelVol)) {            // Right DAC
        Serial.println("Failed to configure DAC volumes!");
      }
      Serial.printf("DAC volume now %.1f\n", channelVol);
    }
    else if (*buf == 'a') {
      // toggle adaptive mode
      dac.setAdaptiveMode(!dac.getAdaptiveMode());
      Serial.printf("adaptive mode is switched %s\n", dac.getAdaptiveMode() ? "on" : "off");
    }
  }
}
