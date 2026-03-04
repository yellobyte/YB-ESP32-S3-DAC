/*
  IIR (1st order) High Pass Filter

  An ESP32 background thread is feeding the TLV320 with a sine tone sweep 50Hz...5kHz.
  The TLV320DAC3101 Stereo Audio DAC has an IIR (1st order) high pass filter activated on
  both audio channels (left & right) and therefore lower frequencies get attenuated.
  The audio signal is output on both the speaker and headphone sockets.

  We use the IIR filter section of processing block PRB_P3.

  The example accepts the following serial input:
   - d...disables filtering,
   - e...enables filtering,
   - a...toggles adaptive mode

  The following additional libraries are needed:
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-02-21, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <ESP_I2S.h>
#include "TLV320DAC3101.h"

// ESP32-S3 I2S bus settings
i2s_mode_t           mode  = I2S_MODE_STD;              // Philips standard
i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;  // 16bit data/sample width
i2s_slot_mode_t      slot  = I2S_SLOT_MODE_STEREO;      // 2 slots (stereo)

// audio definitions for sine tone generation
#define SAMPLERATE_HZ 32000        // audio sample rate (e.g. 32000, 44100, 48000)
#define FREQU_MAX     5000         // highest generated frequency
#define FREQU_MIN     50           // lowest generated frequency
#define FREQU_DELTA   1            // Hz, frequency step
#define INTERVAL      2            // ms, delay before changing to next frequency

// defines the -3dB corner frequency of the high pass filter
#define FREQU_C       2500         // Hz

float amplitude = ((1<<14)-1);     // amplitude of generated waveform
int32_t frequency = FREQU_MIN,     // start frequency of generated waveform
        maxSamples = (int32_t)(SAMPLERATE_HZ / 1000.0 * INTERVAL),
        fdelta = FREQU_DELTA;

// for pre-calculation of sine waveform in memory
#define WAV_SIZE      4096         // size/points of generated waveform
int16_t waveform[WAV_SIZE] = { 0 };

I2SClass  i2s;
TLV320DAC3101 dac;
tlv320_filter_param_t filter;        // keeps filter settings

// Background task continuously feeding I2S bus with sine tone sweep
void backgroundTask(void *parameter) {
  uint16_t pos = 0, delta;
  while (true) {
    // give some visual feedback about actual frequency
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

  Serial.println("\nrunning example \"IIR (1st order) High Pass Filter\":");

  // generate a sine wave signal with defined amplitude in RAM buffer
  for (int i = 0; i < WAV_SIZE; ++i) {
    waveform[i] = int16_t(amplitude * sin(2.0 * PI * ((1.0 / WAV_SIZE) * i)));
  }
  Serial.println("Sine table generated.");

  // TLV320DAC3101 Audio DAC initialization
  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);    // resets the DAC chip
  delay(100);
  digitalWrite(TLV_RESET, HIGH);

  Serial.println("Init TLV320 DAC");
  if (!dac.begin()) {
    halt("Failed to initialize codec!");
  }

  // I2S Interface Control
  if (!dac.setCodecInterface(TLV320DAC3100_FORMAT_I2S,       // Format: I2S (Philips standard)
                             TLV320DAC3100_DATA_LEN_16)) {   // Length: 16 bits
    halt("Failed to configure codec interface!");
  }

  // Clock MUX and PLL settings
  if (!dac.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL) ||  // PLL output feeds Codec
      !dac.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK)) {     // BCLK feeds PLL input
    halt("Failed to configure codec clocks!");
  }

  if (!dac.setPLLValues(1, 2, 48, 0)) {      // Configure PLL dividers P, R, J and D
    halt("Failed to configure PLL values!");
  }

  if (!dac.setNDAC(true, 6) ||               // Configure DAC dividers NDAC, MDAC and DOSR
      !dac.setMDAC(true, 4) ||
      !dac.setDOSR(128)) {
    Serial.println("Failed to configure DAC dividers!");
  }

  if (!dac.powerPLL(true)) {                 // Power up the PLL
    halt("Failed to power up PLL!");
  }

  // PRB_P3 (RC10) contains IIR filtering option
  if (!dac.setDACProcessingBlock(3)) {
    halt("Failed to configure Processing Block!");
  }

  // setting parameters for the IIR filter
  filter.fc = (float)FREQU_C;                // -3dB corner frequency
  // instead of using the function below you could set filter coefficients manually
  // .N0H = 0x75,
  // .N0L = 0x0F,
  // ...

  if (!dac.calcDACFilterCoefficients(SAMPLERATE_HZ, TLV320_FILTER_TYPE_HIGH_PASS,
                                     TLV320_FILTER_IIR, &filter)) {
    halt("Failed to calculate IIR filter coefficients!");
  }

  if (!dac.setDACFilter(true,                    // enable filtering
                        true,                    // on left channel
                        true,                    // and on right channel
                        TLV320_FILTER_IIR,       // using IIR filter block
                        &filter)) {              // pointer to filter settings
    halt("Failed to configure IIR filter!");
  }

  // Configure DAC path - now power up both left and right DACs
  if (!dac.setDACDataPath(true, true,                      // Power up both DACs
                          TLV320_DAC_PATH_NORMAL,          // Normal left path
                          TLV320_DAC_PATH_NORMAL,          // Normal right path
                          TLV320_VOLUME_STEP_1SAMPLE)) {   // Step: 1 per sample
    halt("Failed to configure DAC data path!");
  }

  // Route DAC output to headphone
  if (!dac.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER,   // Left DAC to mixer
                                 TLV320_DAC_ROUTE_MIXER,   // Right DAC to mixer
                                 false, false, false,      // No AIN routing
                                 false)) {                 // No HPL->HPR
    halt("Failed to configure DAC routing!");
  }

  // DAC Volume Control
  if (!dac.setDACVolumeControl(
        false, false, TLV320_VOL_INDEPENDENT) ||   // Unmute both channels
      !dac.setChannelVolume(false, 0) ||           // Left DAC +0dB
      !dac.setChannelVolume(true, 0)) {            // Right DAC +0dB
    halt("Failed to configure DAC volumes!");
  }

  // Headphone & Speaker Setup
  if (!dac.configureHeadphoneDriver(
        true, true,                           // Power up both drivers
        TLV320_HP_COMMON_1_50V,               // Default common mode
        false) ||                             // Don't power down on SCD
      !dac.configureHPL_PGA(0, true) ||       // Set HPL gain (0-9dB), unmute
      !dac.configureHPR_PGA(0, true) ||       // Set HPR gain (0-9dB), unmute
      !dac.setHPLVolume(true, 20) ||          // Enable and set HPL volume: -10dB
      !dac.setHPRVolume(true, 20)) {          // Enable and set HPR volume: -10dB
    halt("Failed to configure headphone outputs!");
  }

  if (!dac.enableSpeaker(true) ||                 // Disable/Enable speaker amps
      !dac.configureSPK_PGA(TLV320_SPK_GAIN_6DB,  // Set gain to 6dB
                            true) ||              // Unmute
      !dac.setSPKVolume(true, 20)) {              // Enable and set volume to -10dB
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

  // The adaptive mode gets enabled with I2S bus already active and DACs powered up.
  dac.setAdaptiveMode(true);
}

void loop() {
  char buf[10];

  // check for serial input and perform the requested action
  if (Serial.read(buf, sizeof(buf))) {
    if (*buf == 'e') {                        // enable filtering
      Serial.println("---> enable high pass filter");
      if (!dac.setDACFilter(true, true, true, TLV320_FILTER_IIR, &filter)) {
        halt("Failed to enable IIR filter!");
      }
    }
    else if (*buf == 'd') {                   // disable filtering
      Serial.println("---> disable high pass filter");
      if (!dac.setDACFilter(false, true, true, TLV320_FILTER_IIR, &filter)) {
        halt("Failed to disable IIR filter!");
      }
    }
    else if (*buf == 'a') {                   // toggle adaptive mode
      dac.setAdaptiveMode(!dac.getAdaptiveMode());
      Serial.printf("adaptive mode is switched %s\n", dac.getAdaptiveMode() ? "on" : "off");
    }
  }
  delay(10);
}
