/*
  Beep Generator

  This example generates short beeps in random intervals with the TLV320DAC3101 integrated
  beep generator while a sound sample is playing from ESP32 flash memory.

  The TLV320 initialization sequence is based on Adafruit_TLV320_I2S lib examples and
  has been modified to fit the TLV320DAC3101 Stereo Audio DAC.

  The following libraries are needed:
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-02-20, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <ESP_I2S.h>
//#include <math.h>
#include "TLV320DAC3101.h"
#include "audioFile-32000fs-16bit.h"       // mono audio file in flash

// ESP32-S3 I2S bus settings
i2s_mode_t           mode  = I2S_MODE_STD;              // Philips standard
i2s_data_bit_width_t width = I2S_DATA_BIT_WIDTH_16BIT;  // 16bit data/sample width
i2s_slot_mode_t      slot  = I2S_SLOT_MODE_STEREO;      // 2 slots (stereo)
I2SClass i2s;

TLV320DAC3101 dac;

void halt(const char *message) {
  Serial.println(message);
  while (true) yield(); // Function to halt on critical errors
}

// background task continuously feeding I2S bus with audio data
void backgroundTask(void *parameter) {
  digitalWrite(LED_BUILTIN, HIGH); // status LED On
  while (true) {
    for (uint32_t i = 0; i < sizeof(audioFile); i += 2) {
      uint8_t sample_low8bit = audioFile[i],
              sample_high8bit = audioFile[i + 1];
      // left channel, low 8 bits first
      i2s.write(sample_low8bit);
      i2s.write(sample_high8bit);
      // right channel, low 8 bits first
      i2s.write(sample_low8bit);
      i2s.write(sample_high8bit);
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
  if (!dac.setCodecInterface(TLV320DAC3100_FORMAT_I2S,       // Format: I2S (Philips standard, default)
                             TLV320DAC3100_DATA_LEN_16)) {   // Length: 16 bits (default)
    halt("Failed to configure codec interface!");
  }
  // Clock MUX and PLL settings
  if (!dac.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL) ||  // PLL output feeds Codec
  !dac.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK)) {         // BCLK feeds PLL input
    halt("Failed to configure codec clocks!");
  }

    // Only processing block PRB_P25 (RC12) contains the Beep Generator
  if (!dac.setDACProcessingBlock(25)) {
    halt("Failed to configure processing block!");
  }

  // Beep generator settings: fBeep = 1000Hz, duration = 100ms
  if (!dac.configureBeepTone(1000.0, 100, SAMPLERATE_HZ)) {
    halt("Failed to configure beep tone!");
  }

  if (!dac.setBeepVolume(-10, -10)) {        // setting beep volume to L/R -10dB
    halt("Failed to configure beep volume!");
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

  // Configure DAC path - power up both left and right DACs
  if (!dac.setDACDataPath(true, true,                      // DACs powered up
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
      !dac.setChannelVolume(false, 3) ||           // Left DAC +3dB
      !dac.setChannelVolume(true, 3)) {            // Right DAC +3dB
    halt("Failed to configure DAC volumes!");
  }

  // Headphone & Speaker Setup
  if (!dac.configureHeadphoneDriver(
        true, true,                           // Power up both drivers
        TLV320_HP_COMMON_1_65V,               // Default common mode
        false) ||                             // Don't power down on SCD
      !dac.configureHPL_PGA(0, true) ||       // Set HPL gain, unmute
      !dac.configureHPR_PGA(0, true) ||       // Set HPR gain, unmute
      !dac.setHPLVolume(true, 20) ||          // Enable and set HPL volume to -10dB
      !dac.setHPRVolume(true, 20)) {          // Enable and set HPR volume to -10dB
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
