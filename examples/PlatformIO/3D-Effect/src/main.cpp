/*
  Play-Radio-Station with 3D effect enabled/disabled

  Play a radio station's audio stream. Output goes to speaker and headphone sockets.
  The 3D effect is initially set and enabled with the BiQuadA blocks in the
  3D section inactive. Please see Readme.md for details.

  The ESP32-audioI2S Lib predefines the sample frequency of 44100Hz.

  The example accepts the following serial input:
   - d...disable 3D effect,
   - e...enable 3D effect, HPF inactive (default)
   - E...enable 3D effect, HPF active
   - +/-...increase/decrease 3D effect
   - a...toggle adaptive mode

  The following libraries are needed:
   - TLV320DAC3101
   - ESP32-audioI2S v3.4.x
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO

  Last updated 2026-03-25, ThJ <yellobyte@bluewin.ch>
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

#define SAMPLERATE_HZ 44100  // predefined by ESP32-audioI2S library
#define FREQU_C       400    // Hz, -3dB corner frequ. of HPF

Audio audio;
TLV320DAC3101 dac;
tlv320_filter_param_t filterA, *p_fA = NULL;
tlv320_init_config_t cfg;
float pgaGain3D = 1.0;       // initial value, allowed range: 0.0...1.0

void halt(const char *message) {
  Serial.println(message);
  while (true) yield();  // Function to halt on critical errors
}

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  Serial.println("\nrunning example \"Play Radio Station with 3D effect enabled/disabled\":");

  // connecting to local WiFi network
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
  cfg.sample_frequency = SAMPLERATE_HZ;      // Hz, must be set
  cfg.dac_gain_left = 5.0;                   // dB, defaults to 0dB when not set,
  cfg.dac_gain_right = 5.0;                  // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg, false)) {           // set registers but keep DACs powered down
    halt(dac.getLastError().c_str());
  }

  // only PRB_P23...PRB_P25 (RC8/RC12/RC12) contain 3D effect option
  if (!dac.setDACProcessingBlock(23)) {
    halt("Failed to configure Processing Block!");
  }

  // calculate coefficients for a BiQuad (2nd order) filter block
  filterA.fc = (float)FREQU_C;               // -3dB corner frequency of HPF
  if (!dac.calcDACFilterCoefficients(SAMPLERATE_HZ,                 // Hz, sample frequency
                                     TLV320_FILTER_TYPE_HIGH_PASS,  // filter type: high pass (HPF)
                                     TLV320_FILTER_BIQUAD,          // 2nd order filter (BiQuad)
                                     &filterA)) {                   // keeps all filter settings
    halt("Failed to calculate BiQuad filter coefficients!");
  }

  // setting & enabling 3D effect
  if (!dac.set3D(true,                 // enable 3D effect
                 pgaGain3D,            // set 3D PGA gain (0.0...1.0)
                 NULL,                 // left BiQuadA stays inactive (linear)
                 NULL)) {              // right BiQuadA stays inactive (linear)
    halt("Failed to set 3D effect!");
  }

  // power up both DACs
  if (!dac.powerOnDAC(true, true)) {
    halt("Failed to power on DACs!");
  }

  // activating headphone output and setting headphone volume
  if (!dac.configHeadphoneOutput(true,                // enable headphone output
                                 false,               // HP(L/R) output driver acts as headphone driver
                                 80)) {               // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure headphone output!");
  }

  // activating speaker output and setting speaker volume
  if (!dac.configSpeakerOutput(true,                // enable speaker output
                               100)) {              // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure speaker output!");
  }
  Serial.println("TLV320 DAC config done!");

  audio.audio_info_callback = my_audio_info;
  audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  audio.setVolume(10);                 // 0...21(max)
  audio.setConnectionTimeout(1200,0);  // needed for some stations esp. from around the globe
  audio.connecttohost(RADIO_STREAM);

  // adaptive mode gets enabled with I2S bus active and DACs powered up
  dac.setAdaptiveMode(true);
}

void loop() {
  char buf[10];

  audio.loop();    // play audio stream
  vTaskDelay(1);   // needed by ESP32-audioI2S lib!

  // evaluate serial input
  if (Serial.read(buf, sizeof(buf))) {
    if (*buf == 'e') {
      // enable 3D effect with BiQuads inactive
      p_fA = NULL;
      if (!dac.set3D(true, pgaGain3D, p_fA)) {
        halt("Failed to enable DRC!");
      }
      Serial.println("3D enabled");
    }
    if (*buf == 'E') {
      // enable 3D effect with left BiQuadA active
      p_fA = &filterA;
      if (!dac.set3D(true, pgaGain3D, p_fA)) {
        halt("Failed to enable DRC!");
      }
      Serial.println("3D enabled (left BiquadA active)");
    }
    else if (*buf == 'd') {
      // disable 3D effect
      if (!dac.set3D(false)) {
        halt("Failed to disable 3D!");
      }
      Serial.println("3D disabled");
    }
    else if (*buf == '+') {
      pgaGain3D += 0.1;
      if (pgaGain3D > 1.0) pgaGain3D = 1.0;
      if (!dac.set3D(true, pgaGain3D, p_fA)) {
        Serial.println("Failed to increase 3D effect!");
      }
      Serial.printf("3D PGA gain now %.1f\n", pgaGain3D);
    }
    else if (*buf == '-') {
      pgaGain3D -= 0.1;
      if (pgaGain3D < 0.0) pgaGain3D = 0.0;
      if (!dac.set3D(true, pgaGain3D, p_fA)) {
        Serial.println("Failed to decrease 3D effect!");
      }
      Serial.printf("3D PGA gain now %.1f\n", pgaGain3D);
    }
    else if (*buf == 'a') {           // toggle adaptive mode
      dac.setAdaptiveMode(!dac.getAdaptiveMode());
      Serial.printf("adaptive mode is switched %s\n", dac.getAdaptiveMode() ? "on" : "off");
    }
  }
}
