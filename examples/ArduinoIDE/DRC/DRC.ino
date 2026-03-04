/*
  Play Audio from SD with DRC activated/deactivated

  This example plays all mp3-files from microSD card. Output goes to speaker and headphone sockets.
  DRC (Dynamic Range Compression) is set and enabled.

  Since "SD" library is used which requires the cards CS signal (GPIO10) the solder bridge
  SD_CS must be closed [default].

  The TLV320 initialization sequence is based on Adafruit_TLV320_I2S lib examples and
  has been modified to fit the TLV320DAC3101 Stereo Audio DAC.

  The example accepts the following serial input:
   - d...disables DRC,
   - e...enables DRC,
   - +/-...increases/decreases volume
   - a...toggles adaptive mode

  The following libraries are needed:
   - SD
   - ESP32-audioI2S v3.4.x
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-02-20, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "Audio.h"
#include "TLV320DAC3101.h"

#define MAX_PATH_DEPTH 4

TLV320DAC3101 dac;
tlv320_drc_param_t drc;
Audio audio;
SPIClass *spi_onboardSD = new SPIClass(FSPI);
File root, entry;
std::vector<File> dirChain;
float channelVol = 10.0;
bool playing = false;

// non-standard DRC LPF/HPF coefficients as used in example in Ch. 6.3.10.4.6
uint8_t my_drc_lpf_coeffs[6] = {0x00, 0x11, 0x00, 0x11, 0x7F, 0xDE};
uint8_t my_drc_hpf_coeffs[6] = {0x7F, 0xAB, 0x80, 0x55, 0x7F, 0x56};

void halt(const char *message) {
  Serial.println(message);
  while (true) yield();  // Function to halt on critical errors
}

void searchAudioFiles() {
  while (!playing && dirChain.size()) {
    entry = dirChain[dirChain.size() - 1].openNextFile();
    if (!entry) {
      // no (more) files in this directory
      dirChain[dirChain.size() - 1].close();
      dirChain.pop_back();
      break;
    }
    if (entry.isDirectory()) {
      if (dirChain.size() < MAX_PATH_DEPTH) {
        dirChain.push_back(entry);  // dir entry stays open while member of chain
        break;
      }
    }
    else if (String(entry.name()).endsWith("mp3")) {
      Serial.print("now playing: ");
      Serial.println(String(entry.path()));
      audio.connecttoFS(SD, entry.path());
      entry.close();
      playing = true;
      break;
    }
    entry.close();
  }
}

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
    if ( m.e == Audio::evt_eof ) playing = false;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  delay(100);

  Serial.println("\nrunning example \"Play Audio from microSD with DRC activated/deactivated\":");

  spi_onboardSD->begin(SCK, MISO, MOSI, SS);

  if (!SD.begin(SS, *spi_onboardSD)) {
    Serial.println("error mounting microSD");
    return;
  }
  digitalWrite(LED_BUILTIN, HIGH); // status LED On
  Serial.println("Mounting microSD ok");

  root = SD.open("/");
  dirChain.push_back(root);

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
      !dac.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK)) {     // BCLK feeds PLL input
    halt("Failed to configure codec clocks!");
  }

  if (!dac.setPLLValues(1, 2, 32, 0)) {      // Configure PLL dividers P, R, J and D
    halt("Failed to configure PLL values!");
  }

  if (!dac.setNDAC(true, 4) ||               // Configure DAC dividers NDAC, MDAC and DOSR, for RC>8
      !dac.setMDAC(true, 4) ||
      !dac.setDOSR(128)) {
    Serial.println("Failed to configure DAC dividers!");
  }

  if (!dac.powerPLL(true)) {                 // Power up the PLL
    halt("Failed to power up PLL!");
  }

  // Configure DAC path - power up both left and right DACs
  if (!dac.setDACDataPath(false, false,                    // DACs not powered up yet
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
        false, false, TLV320_VOL_INDEPENDENT) ||            // Unmute both channels
      !dac.setChannelVolume(false, channelVol) ||           // Left DAC +10dB
      !dac.setChannelVolume(true, channelVol)) {            // Right DAC +10dB
    halt("Failed to configure DAC volumes!");
  }

  // PRB_P2 (RC12) contains DRC filtering option
  if (!dac.setDACProcessingBlock(2)) {
    halt("Failed to configure Processing Block!");
  }

  // setting & enabling DRC with some non-standard parameters
  drc.hyst = TLV320_DRC_HYST_2DB;
  drc.lpf_coeffs = my_drc_lpf_coeffs;
  drc.hpf_coeffs = my_drc_hpf_coeffs;

  if (!dac.setDRC(true,                // enable DRC
                  true,                // on left channel and
                  true,                // on right channel
                  &drc)) {             // NULL for standard parameters
    halt("Failed to configure DRC!");
  }

  // Power on both DACs
  if (!dac.powerOnDAC(true, true)) {
    halt("Failed to power on DACs!");
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

  audio.audio_info_callback = my_audio_info;
  audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  audio.setVolume(10); // 0...21(max)

  // The adaptive mode gets enabled with I2S bus active and DACs powered up.
  dac.setAdaptiveMode(true);
}

void loop() {
  if (!playing) {
    searchAudioFiles();
  }
  audio.loop();  // play mp3 audio file
  vTaskDelay(1); // needed by ESP32-audioI2S lib!

  char buf[10];
  if (Serial.read(buf, sizeof(buf))) {
    if (*buf == 'e') {
      // enable DRC
      if (!dac.setDRC(true, true, true, &drc)) {
        halt("Failed to enable DRC!");
      }
      Serial.println("DRC enabled");
    }
    else if (*buf == 'd') {
      // disable DRC
      if (!dac.setDRC(false, true, true, NULL)) {
        halt("Failed to disable DRC!");
      }
      Serial.println("DRC disabled");
    }
    else if (*buf == '+') {
      channelVol += 1.0;
      if (channelVol > 24.0) channelVol = 24.0;
      if (!dac.setChannelVolume(false, channelVol) ||           // Left DAC
          !dac.setChannelVolume(true, channelVol)) {            // Right DAC
        Serial.println("Failed to configure DAC volumes!");
      }
      Serial.printf("DAC volume now %.1f\n", channelVol);
    }
    else if (*buf == '-') {
      channelVol -= 1.0;
      if (channelVol < -63.0) channelVol = -63.0;
      if (!dac.setChannelVolume(false, channelVol) ||           // Left DAC
          !dac.setChannelVolume(true, channelVol)) {            // Right DAC
        Serial.println("Failed to configure DAC volumes!");
      }
      Serial.printf("DAC volume now %.1f\n", channelVol);
    }
    else if (*buf == 'a') {           // toggle adaptive mode
      dac.setAdaptiveMode(!dac.getAdaptiveMode());
      Serial.printf("adaptive mode is switched %s\n", dac.getAdaptiveMode() ? "on" : "off");
    }
  }
}




