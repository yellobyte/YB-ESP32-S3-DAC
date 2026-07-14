/*
  BiQuad Treble Shelf Filter (4th order)

  An ESP32 background thread is feeding the TLV320 with a sine tone sweep 50Hz...5000Hz.
  The TLV320DAC3101 Stereo Audio DAC has two cascaded BiQuad Treble Shelf filter blocks with
  fc=2kHz and gain=+10dB activated. Therefore the frequency spectrum above 2kHz gets a
  constant boost. The audio signal is output on both the speaker and headphone sockets.

  Processing block PRB_P1 (default) contains 3 BiQuad filter blocks (A, B, C). We configure
  and use two of them.

  The example accepts the following serial input:
   - d...disables the filter,
   - e...enables the filter,
   - a...toggles adaptive mode

  The following additional libraries are needed:
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-07-14, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <ESP_I2S.h>
#include "TLV320DAC3101.h"

// ESP32-S3 I2S settings
i2s_mode_t           mode  = I2S_MODE_STD;              // Philips standard
i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;  // 16bit data/sample width
i2s_slot_mode_t      slot  = I2S_SLOT_MODE_STEREO;      // 2 slots (stereo)

// audio definitions
#define SAMPLERATE_HZ 44100        // Hz, audio sample rate (e.g. 32000, 44100, 48000)
#define FREQU_MAX     5000         // Hz, highest generated frequency
#define FREQU_MIN     50           // Hz, lowest generated frequency
#define FREQU_DELTA   1            // Hz, frequency step
#define INTERVAL      1            // ms, delay before changing to next frequency

// defines the parameters of each BiQuad treble shelf filter block
#define FREQU_C       2000         // Hz, frequencies above 2kHz get a constant boost
#define GAIN          10.0         // dB, constant filter block gain at frequencies above fc,
                                   // Note: setting the overall gain to high might cause
                                   // the filter to become unstable!

float amplitude = ((1<<14)-1);     // amplitude of generated waveform
uint32_t frequency = FREQU_MIN,    // start frequency of generated waveform
         maxSamples = (int32_t)(SAMPLERATE_HZ / 1000.0 * INTERVAL),
         fdelta = FREQU_DELTA;

// for pre-calculation of sine waveform in memory
#define WAV_SIZE      4096         // size/points of generated waveform
int16_t waveform[WAV_SIZE] = {0};

I2SClass i2s;
TLV320DAC3101 dac;
tlv320_init_config_t cfg;
tlv320_filter_param_t filter;      // keeps the filter parameter

// Background task continuously feeding DAC with sine tone sweep
void backgroundTask(void *parameter) {
  uint16_t pos = 0, delta;
  while (true) {
    // give some visual feedback about actual frequency
    if (!(frequency % 100)) Serial.printf("frequency=%.0luHz\n", frequency);

    // generate sine tone sweep
    delta = (uint16_t)(frequency * (float)WAV_SIZE / (float)SAMPLERATE_HZ);
    for (uint32_t i = 0; i < maxSamples; ++i) {
      pos = uint16_t(pos + delta) % (uint16_t)WAV_SIZE;
      int16_t sample = waveform[pos];
      // left channel
      i2s.write((uint8_t *)&sample, sizeof(sample));
      // right channel
      i2s.write((uint8_t *)&sample, sizeof(sample));
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

  Serial.println("\nrunning example \"Treble Shelf Filter\":");

  // generate a sine wave signal with defined amplitude in RAM buffer
  for (int i = 0; i < WAV_SIZE; ++i) {
    waveform[i] = int16_t(amplitude * sin(2.0 * PI * ((1.0 / WAV_SIZE) * i)));
  }
  Serial.println("Sine table generated.");

  // HW reset makes sure DAC chip is reset properly
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(10);
  digitalWrite(TLV_RESET, HIGH);

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = SAMPLERATE_HZ;      // Hz, must be set
  cfg.dac_gain_left = -10.0;                 // dB, defaults to 0dB when not set,
  cfg.dac_gain_right = -10.0;                // allowed range: -63.5...+24.0 dB

  if (!dac.initDAC(&cfg, false)) {           // set registers but keep DACs powered down
    halt(dac.getLastError().c_str());
  }

  // set parameters for treble shelf filter
  filter.fc = (float)FREQU_C;
  filter.gain = (float)GAIN;
  // instead of using the function below one could set filter coefficients manually
  // filter.N0H = 0x3F,
  // filter.N0L = 0xF2,
  // ...

  // calculate coefficients for Biquad filter blocks
  if (!dac.calcDACFilterCoefficients(SAMPLERATE_HZ, TLV320_FILTER_TYPE_TREBLE_SHELF,
                                     TLV320_FILTER_BIQUAD, &filter)) {
    halt("Failed to calculate BiQuad filter coefficients!");
  }

  if (!dac.setDACFilter(true,                    // enable filtering
                        true,                    // on left channel
                        true,                    // and on right channel
                        TLV320_FILTER_BIQUAD_A,  // using BiQuadA filter block
                        &filter) ||              // pointer to filter settings
      !dac.setDACFilter(true,                    // enable filtering
                        true,                    // on left channel
                        true,                    // and on right channel
                        TLV320_FILTER_BIQUAD_B,  // using BiQuadB filter block
                        &filter)) {              // pointer to filter settings
    halt("Failed to set BiQuad filter blocks!");
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
  delay(50);

  // adaptive mode gets enabled with I2S bus already active and DACs powered up
  dac.setAdaptiveMode(true);
}

void loop() {
  char buf[10];

  // check for serial input and perform the requested action
  if (Serial.read(buf, sizeof(buf))) {
    if (*buf == 'e') {                        // enable filtering
      Serial.println("---> enable treble shelf filter");
      if (!dac.setDACFilter(true, true, true, TLV320_FILTER_BIQUAD_A, &filter) ||
          !dac.setDACFilter(true, true, true, TLV320_FILTER_BIQUAD_B, &filter)) {
        halt("Failed to enable filtering!");
      }
    }
    else if (*buf == 'd') {                   // disable filtering
      Serial.println("---> disable treble shelf filter");
      if (!dac.setDACFilter(false, true, true, TLV320_FILTER_BIQUAD_A) ||
          !dac.setDACFilter(false, true, true, TLV320_FILTER_BIQUAD_B)) {
        halt("Failed to disable filtering!");
      }
    }
    else if (*buf == 'a') {                   // toggle adaptive mode
      dac.setAdaptiveMode(!dac.getAdaptiveMode());
      Serial.printf("adaptive mode is switched %s\n", dac.getAdaptiveMode() ? "on" : "off");
    }
  }
  delay(10);
}
