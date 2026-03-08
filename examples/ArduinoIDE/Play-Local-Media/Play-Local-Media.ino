/*
  Play-Local-Media

  Plays a media file from a local DLNA server. Output goes to speaker and headphone sockets.

  The ESP32-audioI2S Lib predefines the sample frequency of 44100Hz.

  The following libraries are needed:
   - ESP32-audioI2S 3.4.x
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO

  Last updated 2026-03-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include "Audio.h"
#include "TLV320DAC3101.h"

// media files hosted on a local DLNA server, e.g.:
#define MEDIA_FILE "http://192.168.1.11:9000/disk/DLNA-PNMP3-OP01-FLAGS01700000/O0$1$8I2534412.mp3"
//#define MEDIA_FILE "http://192.168.1.40:9790/*/H*c3*b6rb*c3*bccher/A01/A*20C*20M/1-01*20Titel*2001.mp3"

const char ssid[] = "MySSID";
const char pass[] = "MyPassword";

TLV320DAC3101 dac;
tlv320_init_config_t cfg;
Audio audio;

void halt(const char *message) {
  Serial.println(message);
  while (true) yield(); // Function to halt on critical errors
}

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("running example \"Play-Local-Media\":");

  // connect to local WiFi network
  Serial.printf("connecting to WiFi network \"%s\"\n", ssid);
  WiFi.begin(ssid, pass);
  while ( WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(2000);
  }
  Serial.printf("\nconnected successfully to \"%s\". IP address: %s\n",
                   ssid, WiFi.localIP().toString().c_str());

  // HW reset makes sure DAC chip is reset properly
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(10);
  digitalWrite(TLV_RESET, HIGH);

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = 44100.0;             // Hz, must be set
  cfg.dac_gain_left = 10.0;                   // dB, defaults to 0dB when not set,
  cfg.dac_gain_right = 10.0;                  // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg)) {
    halt(dac.getLastError().c_str());
  }

  // activate headphone output and set headphone volume
  if (!dac.configHeadphoneOutput(true,              // enable headphone output
                                 false,             // HP(L/R) output driver acts as headphone driver
                                 90)) {             // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure headphone output!");
  }

  // activate speaker output and set speaker volume
  if (!dac.configSpeakerOutput(true,              // enable speaker output
                               100)) {            // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure speaker output!");
  }
  Serial.println("TLV320 DAC config done!");

  audio.audio_info_callback = my_audio_info;
  audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  audio.setVolume(12);                 // 0...21(max)
  audio.setConnectionTimeout(500,0);   // needed for some servers
  audio.connecttohost(MEDIA_FILE);
  delay(100);
}

void loop()
{
  audio.loop();      // play audio stream
  vTaskDelay(1);     // needed for ESP32-audioI2S lib!
}



