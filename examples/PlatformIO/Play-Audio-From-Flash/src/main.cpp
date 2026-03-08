/*
  Play Audio from Flash

  This example configures the TLV320DAC3101 I2S Stereo Audio DAC and the ESP32 I2S bus and then
  plays a short audio from flash on both the speaker and headphone outputs.

  The following additional libraries are needed:
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-03-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <ESP_I2S.h>
#include "TLV320DAC3101.h"
#include "audioFile_16000Hz_16bit_mono.h"   // audio file in flash

// ESP32-S3 I2S settings
i2s_mode_t           mode  = I2S_MODE_STD;              // Philips standard
i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;  // 16bit data/sample width
i2s_slot_mode_t      slot  = I2S_SLOT_MODE_STEREO;      // 2 slots (stereo)
I2SClass i2s;

TLV320DAC3101 dac;
tlv320_init_config_t cfg;

// helper function to halt on critical errors
void halt(const char *message) {
  Serial.println(message);
  while (true) yield();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\nrunning example \"Play Audio From Flash\":");

  // HW reset makes sure DAC chip is reset properly
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(10);
  digitalWrite(TLV_RESET, HIGH);

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = SAMPLERATE_HZ;      // Hz, must be set
  //cfg.dac_gain_left = -15.0;                 // dB, defaults to 0dB when not set,
  //cfg.dac_gain_right = -15.0;                // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg)) {
    halt("Failed to initialize DAC core!");
  }

  // activating headphone output and setting headphone volume
  if (!dac.initHeadphoneOutput(true,                // headphone output enabled
                               false,               // HP(L/R) output driver acts as headphone driver
                               80)) {               // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure headphone output!");
  }

  // activating speaker output and setting speaker volume
  if (!dac.initSpeakerOutput(true,                // speaker output enabled
                             100)) {              // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure speaker output!");
  }
  Serial.println("TLV320 DAC config done!");

  // I2S initialization
  i2s.setPins(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  if (!i2s.begin(mode, (uint32_t)SAMPLERATE_HZ, width, slot)) {
    halt("Failed to initialize I2S!");
  }
  Serial.println("I2S initialization done!");
}

void loop() {
  for (uint32_t i = 0; i < sizeof(audioFile); i += 2) {
    // left channel, low 8 bits first
    i2s.write(audioFile[i]);
    i2s.write(audioFile[i + 1]);
    // right channel, low 8 bits first
    i2s.write(audioFile[i]);
    i2s.write(audioFile[i + 1]);
  }
  //vTaskDelay(1);
}
