/*
  Beep Generator

  This example generates short beeps in random intervals with the TLV320DAC3101 integrated
  beep generator while a sound sample from ESP32 flash is playing.

  The following libraries are needed:
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-07-14, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <ESP_I2S.h>
#include "TLV320DAC3101.h"
#include "audioFile-32000fs-16bit.h"       // mono audio file in flash

// ESP32-S3 I2S bus settings
i2s_mode_t           mode  = I2S_MODE_STD;              // Philips standard
i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;  // 16bit data/sample width
i2s_slot_mode_t      slot  = I2S_SLOT_MODE_STEREO;      // 2 slots (stereo)
I2SClass i2s;

TLV320DAC3101 dac;
tlv320_init_config_t cfg;

void halt(const char *message) {
  Serial.println(message);
  while (true) yield(); // Function to halt on critical errors
}

// background task continuously feeding DAC with audio data
void backgroundTask(void *parameter) {
  digitalWrite(LED_BUILTIN, HIGH); // status LED On
  while (true) {
    for (uint32_t i = 0; i < sizeof(audioFile); i += 2) {
      // left channel
      i2s.write((uint8_t *)&audioFile[i], 2);
      // right channel
      i2s.write((uint8_t *)&audioFile[i], 2);
    }
  }
  vTaskDelete(NULL); // will never get here
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);  // status LED Off

  Serial.begin(115200);
  delay(100);

  Serial.println("\nrunning example \"Beep Generator\":");

  // HW reset makes sure DAC chip is reset properly
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);    // resets the DAC chip
  delay(10);
  digitalWrite(TLV_RESET, HIGH);

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = SAMPLERATE_HZ;  // Hz, must be set
  cfg.dac_gain_left = 5.0;               // dB, defaults to 0dB when not set,
  cfg.dac_gain_right = 5.0;              // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg)) {
    halt(dac.getLastError().c_str());
  }

  // only processing block PRB_P25 (RC12) contains the Beep generator
  if (!dac.setDACProcessingBlock(25)) {
    halt("Failed to configure processing block!");
  }

  // config Beep generator
  if (!dac.configureBeepTone(1000.0, 100, SAMPLERATE_HZ) ||  // fBeep = 1000Hz, duration = 100ms
      !dac.setBeepVolume(-15, -15)) {                        // beep volume L/R = -15dB
    halt("Failed to configure beep settings!");
  }

  // activate headphone output and set headphone volume
  if (!dac.configHeadphoneOutput(true,              // enable headphone output
                                 false,             // HP(L/R) output driver acts as headphone driver
                                 90)) {             // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure headphone output!");
  }

  // activate speaker output and set speaker volume
  if (!dac.configSpeakerOutput(true,              // enable speaker output
                               115)) {            // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure speaker output!");
  }
  Serial.println("TLV320 DAC config done!");

  // I2S bus initialization
  i2s.setPins(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  if (!i2s.begin(mode, (uint32_t)SAMPLERATE_HZ, width, slot)) {
    halt("Failed to initialize I2S bus!");
  }
  Serial.println("I2S bus initialization done!");

  xTaskCreate(backgroundTask, "bgTask", 4096, NULL, 1, NULL);
  delay(1000);
}

void loop() {
  delay(random(250, 4000));
  if (!dac.enableBeep(true)) {
    halt("Failed to enable beep generator!");
  }
  else {
    Serial.println("Beep !");
  }
}




