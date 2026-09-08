/*
  Headset detection

  This example initializes the TLV320DAC3101 chip and then monitors the 3.5mm jack
  for headset insertion/removal and/or pressed headset button.

  The following libraries are needed:
   - Adafruit_BusIO
   - Adafruit_TLV320_I2S
   - TLV320DAC3101

  Last updated 2026-09-08, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <TLV320DAC3101.h>

#define SAMPLERATE_HZ 44100      // Hz
TLV320DAC3101 dac;
tlv320_init_config_t cfg;

void halt(const char *message) {
  Serial.println(message);
  while (true) yield();          // Function to halt on critical errors
}

void setup() {
  Serial.begin(115200);

  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(100);
  digitalWrite(TLV_RESET, HIGH);

  Serial.println("\nExample \"headset detection\" is running:");

  // TLV320DAC3101 Audio DAC initialization
  cfg.sample_frequency = SAMPLERATE_HZ;      // must be set

  if (!dac.initDAC(&cfg)) {
    halt(dac.getLastError().c_str());
  }

  // activate headphone output and set headphone volume
  if (!dac.configHeadphoneOutput(true,              // headphone output enabled
                                 false,             // HP(L/R) output driver acts as headphone driver
                                 80)) {             // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure headphone output!");
  }

  // activate speaker output and set speaker volume
  if (!dac.configSpeakerOutput(true,                // speaker output enabled
                               100)) {              // set volume (allowed range: 0(quiet)...127(loud))
    halt("Failed to configure speaker output!");
  }

  // headphone detection setup
  if (!dac.configMicBias(false,                       // disable power down
                         true,                        // always on
                         //TLV320_MICBIAS_2V) ||        // MICBIAS voltage = 2V
                         TLV320_MICBIAS_2_5V) ||      // MICBIAS voltage = 2.5V
                         //TLV320_MICBIAS_AVDD) ||      // MICBIAS voltage = AVDD
      !dac.configDelayDivider(false) ||               // needed when MCLK is not available
      !dac.setHeadsetDetect(true,                     // enable headset detection
                            TLV320_DEBOUNCE_256MS,    // for better glitch rejection
                            TLV320_BTN_DEBOUNCE_32MS)) {
    halt("Failed to configure headset detect");
  }
  Serial.println("TLV320 DAC config done!");
}

void loop() {
  static tlv320_headset_status_t last_status = TLV320_HEADSET_NONE;

  tlv320_headset_status_t status = dac.getHeadsetStatus();
  if (last_status != status) {
    Serial.printf("New status value: 0x%02x ", status);
    switch (status) {
    case TLV320_HEADSET_NONE:
      Serial.println("- Headset removed");
      break;
    case TLV320_HEADSET_WITHOUT_MIC:
      Serial.println("- Headphone detected");
      break;
    case TLV320_HEADSET_WITH_MIC:
      Serial.println("- Headset with mic detected");
      break;
    }
    last_status = status;
  }

  // read sticky IRQ flags
  uint8_t flags = dac.readIRQflags(true);

  // only print if flags are set
  if (flags) {
    Serial.printf("New IRQ flag value: 0x%02x\n", flags);

    if (flags & TLV320DAC3100_IRQ_BUTTON_PRESS) {
      Serial.println(" - Headset button pressed");
    }

    if (flags & TLV320DAC3100_IRQ_HEADSET_DETECT) {
      Serial.println(" - Headset insertion or removal detected");
    }
  }
  delay(100);
}
