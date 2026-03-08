/*
  Play-Radio-Station

  Play a radio station's audio stream. Output goes to speaker and headphone sockets.

  The ESP32-audioI2S Lib predefines the sample frequency of 44100Hz.

  The following libraries are needed:
   - ESP32-audioI2S
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-03-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <WiFi.h>
#include "Audio.h"
#include "TLV320DAC3101.h"

//#define RADIO_STREAM "http://legacy.scahw.com.au/2classicrock_32"
//#define RADIO_STREAM "http://stream.srg-ssr.ch/m/rsp/mp3_128"
//#define RADIO_STREAM "http://www.radioeins.de/frankfurt/livemp3"
//#define RADIO_STREAM "http://vis.media-ice.musicradio.com/CapitalMP3"
#define RADIO_STREAM "http://stream.laut.fm/oldies"

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
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  Serial.println("\nrunning example \"Play-Radio-Station\":");

  // connect to local WiFi network
  Serial.printf("connecting to WiFi network \"%s\"\n", ssid);
  WiFi.begin(ssid, pass);
  while ( WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(2000);
  }
  Serial.printf("\nconnected successfully to \"%s\". IP address: %s\n",
                   ssid, WiFi.localIP().toString().c_str());
  digitalWrite(LED_BUILTIN, HIGH);      // status LED On

  // HW reset makes sure DAC chip is reset properly
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(10);
  digitalWrite(TLV_RESET, HIGH);

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = 44100.0;            // Hz, must be set
  cfg.dac_gain_left = 5.0;                   // dB, defaults to 0dB when not set,
  cfg.dac_gain_right = 5.0;                  // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg)) {
    halt(dac.getLastError().c_str());
  }

  // activate headphone output and set headphone volume
  if (!dac.configHeadphoneOutput(true,              // enable headphone output
                                 false,             // HP(L/R) output driver acts as headphone driver
                                 80)) {             // set volume (allowed range: 0(quiet)...127(loud))
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
  audio.setConnectionTimeout(1200,0);  // needed for some stations esp. from around the globe
  audio.connecttohost(RADIO_STREAM);
}

void loop()
{
  audio.loop();     // play audio stream
  vTaskDelay(1);    // needed by ESP32-audioI2S lib
}



