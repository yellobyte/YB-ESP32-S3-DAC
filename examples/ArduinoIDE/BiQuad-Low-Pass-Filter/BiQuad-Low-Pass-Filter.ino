/*
  BiQuad Low Pass Filter (of 6th order)

  An ESP32 background thread is feeding the TLV320 with a sine tone sweep 50Hz...5kHz.
  The TLV320DAC3101 Stereo Audio DAC has three BiQuad (2nd order) low pass filters cascaded
  pro channel. Together they form a low pass filter of 6th order per channel, which has
  a much steeper filter curve than a single BiQuad filter alone. Therefore frequencies above
  the set corner frequency get strongly attenuated. Q is chosen differently to keep -3dB
  attenuation at fc. The audio signal is output on both the speaker and headphone sockets.

  Processing block PRB_P1 (default) contains 3 BiQuad filter blocks (A, B, C) per channel.
  We configure and use all of them.

  The example accepts the following serial input:
   - d...disable filtering,
   - e...enable filtering,
   - a...toggle adaptive mode

  The following additional libraries are needed:
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-03-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <ESP_I2S.h>
#include "TLV320DAC3101.h"

// ESP32-S3 I2S bus settings
i2s_mode_t           mode  = I2S_MODE_STD;              // Philips standard
i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;  // 16bit data/sample width
i2s_slot_mode_t      slot  = I2S_SLOT_MODE_STEREO;      // 2 slots (stereo)

// audio definitions for sine tone generation
#define SAMPLERATE_HZ 44100         // Hz, audio sample rate (e.g. 32000, 44100, 48000)
#define FREQU_MAX     5000          // Hz, highest generated frequency
#define FREQU_MIN     50            // Hz, lowest generated frequency
#define FREQU_DELTA   1             // Hz, frequency step
#define INTERVAL      2             // ms, delay before changing to next frequency
#define AMPLITUDE     ((1<<14)-1)   // amplitude of generated waveform

// defines the -3dB corner frequency of the low pass filter
#define FREQU_C       1500          // Hz

int32_t frequency = FREQU_MIN,      // start frequency of generated waveform
        maxSamples = (int32_t)(SAMPLERATE_HZ / 1000.0 * INTERVAL),
        fdelta = FREQU_DELTA;

// for pre-calculation of sine waveform in memory
#define WAV_SIZE      8192          // size/points of generated waveform
int16_t waveform[WAV_SIZE] = {0};

I2SClass i2s;
TLV320DAC3101 dac;
tlv320_init_config_t cfg;
tlv320_filter_param_t filterA, filterB, filterC;  // keep the filter parameters

// Background task continuously feeding DAC with sine tone sweep
void backgroundTask(void *parameter) {
  uint16_t pos = 0, delta;

  while (true) {
    // just to give some visual feedback about actual frequency
    if (!(frequency % 100)) Serial.printf("frequency=%.0luHz\n", frequency);

    // generate sine tone sweep
    delta = (int16_t)(frequency * (float)WAV_SIZE / (float)SAMPLERATE_HZ);
    for (uint32_t i = 0; i < maxSamples; ++i) {
      pos = uint16_t(pos + delta) % (uint16_t)WAV_SIZE;
      int16_t sample = waveform[pos];
      // left channel, low 8 bits first
      i2s.write((uint8_t)sample);
      i2s.write((uint8_t)(sample >> 8));
      // right channel, low 8 bits first
      i2s.write((uint8_t)sample);
      i2s.write((uint8_t)(sample >> 8));
    }
    frequency += fdelta;           // set new frequency
    if (frequency <= FREQU_MIN || frequency >= FREQU_MAX) {
      fdelta = -fdelta;            // reverse direction
    }
  }
  vTaskDelete(NULL);               // will never get here
}

// helper function to halt on critical errors
void halt(const char *message) {
  Serial.println(message);
  while (true) yield();
}

void setup() {
  Serial.begin(115200);

  Serial.println("\nrunning example \"Low Pass Filter (6th order)\":");

  // generate a sine wave signal with defined amplitude in RAM buffer
  for (int i = 0; i < WAV_SIZE; ++i) {
    waveform[i] = int16_t((float)AMPLITUDE * sin(2.0 * PI * ((1.0 / WAV_SIZE) * i)));
  }
  Serial.println("Sine table generated.");

  // HW reset makes sure DAC chip is reset properly
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(10);
  digitalWrite(TLV_RESET, HIGH);

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = SAMPLERATE_HZ;      // Hz, must be set
  //cfg.dac_gain_left = -5.0;                  // dB, defaults to 0dB when not set,
  //cfg.dac_gain_right = -5.0;                 // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg, false)) {           // set registers but keep DACs powered down
    halt(dac.getLastError().c_str());
  }

  // set filter parameters for the 3 cascaded filter blocks
  filterA.fc = filterB.fc = filterC.fc = (float)FREQU_C;  // -3dB corner frequency
  filterA.Q = 1 / 0.517638;                  // Qs are different, see Readme.md for explanation
  filterB.Q = 1 / 1.414214;
  filterC.Q = 1 / 1.931852;
  // instead of using the function below one could set filter coefficients manually
  // filterA.N0H = 0x7F,
  // filterA.N0L = 0xFF,
  // ...

  // calculate coefficients for Biquad filter blocks
  if (!dac.calcDACFilterCoefficients(SAMPLERATE_HZ, TLV320_FILTER_TYPE_LOW_PASS,
                                    TLV320_FILTER_BIQUAD, &filterA) ||
      !dac.calcDACFilterCoefficients(SAMPLERATE_HZ, TLV320_FILTER_TYPE_LOW_PASS,
                                    TLV320_FILTER_BIQUAD, &filterB) ||
      !dac.calcDACFilterCoefficients(SAMPLERATE_HZ, TLV320_FILTER_TYPE_LOW_PASS,
                                    TLV320_FILTER_BIQUAD, &filterC)) {
    halt("Failed to calculate BiQuad filter coefficients!");
  }

  if (!dac.setDACFilter(true,                    // enable filtering
                        true,                    // on left channel
                        true,                    // and on right channel
                        TLV320_FILTER_BIQUAD_A,  // using BiQuadA filter
                        &filterA)) {             // pointer to filter settings
    halt("Failed to set BiQuadA filter!");
  }

  if (!dac.setDACFilter(true,                    // enable filtering
                        true,                    // on left channel
                        true,                    // and on right channel
                        TLV320_FILTER_BIQUAD_B,  // using BiQuadB filter
                        &filterB)) {             // pointer to filter settings
    halt("Failed to set BiQuadB filter!");
  }
  
  if (!dac.setDACFilter(true,                    // enable filtering
                        true,                    // on left channel
                        true,                    // and on right channel
                        TLV320_FILTER_BIQUAD_C,  // using BiQuadC filter
                        &filterC)) {             // pointer to filter settings
    halt("Failed to set BiQuadC filter!");
  }

  // filter coeffs are written, now power up DACs
  if (!dac.powerOnDAC(true, true)) {
    halt("Failed to power up DACs!");
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

  // I2S bus initialization
  i2s.setPins(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  if (!i2s.begin(mode, (uint32_t)SAMPLERATE_HZ, width, slot)) {
    halt("Failed to initialize I2S bus!");
  }
  Serial.println("I2S bus initialization done!");

  xTaskCreate(backgroundTask, "bgTask", 4096, NULL, 1, NULL);
  delay(100);

  // adaptive mode gets enabled with I2S bus already active and DACs powered up
  dac.setAdaptiveMode(true);
}

void loop() {
  char buf[10];

  // check for serial input and perform the requested action
  if (Serial.read(buf, sizeof(buf))) {
    if (*buf == 'e') {                // enable filtering on left and right channel
      Serial.println("---> enable low pass filtering");
      // if (!dac.setDACFilter(true, true, true, TLV320_FILTER_BIQUAD_A, &filter)) {
      if (!dac.setDACFilter(true, true, true, TLV320_FILTER_BIQUAD_A, &filterA) ||
          !dac.setDACFilter(true, true, true, TLV320_FILTER_BIQUAD_B, &filterB) ||
          !dac.setDACFilter(true, true, true, TLV320_FILTER_BIQUAD_C, &filterC)) {
        Serial.println("Failed to enable filtering !");
      }
    }
    else if (*buf == 'd') {           // disable filtering on left and right channel
      Serial.println("---> disable low pass filtering");
      // if (!dac.setDACFilter(false, true, true, TLV320_FILTER_BIQUAD_A, NULL)) {
      if (!dac.setDACFilter(false, true, true, TLV320_FILTER_BIQUAD_A) ||
          !dac.setDACFilter(false, true, true, TLV320_FILTER_BIQUAD_B) ||
          !dac.setDACFilter(false, true, true, TLV320_FILTER_BIQUAD_C)) {
        Serial.println("Failed to disable filtering !");
      }
    }
    else if (*buf == 'a') {           // toggle adaptive mode
      dac.setAdaptiveMode(!dac.getAdaptiveMode());
      Serial.printf("adaptive mode is switched %s\n", dac.getAdaptiveMode() ? "on" : "off");
    }
  }
  delay(10);
}
