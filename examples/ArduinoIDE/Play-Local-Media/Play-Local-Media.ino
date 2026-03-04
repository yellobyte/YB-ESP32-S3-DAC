/*
  Play-Local-Media

  Plays a media file from a local DLNA server. Output goes to speaker and headphone sockets.

	The TLV320 initialization sequence is based on Adafruit_TLV320_I2S lib examples and
	has been modified to fit the TLV320DAC3101 Stereo Audio DAC on the YB-ESP32-S3-DAC board.  
  
  The following libraries are needed:
   - ESP32-audioI2S
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-01-10, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include "TLV320DAC3101.h"
#include "Audio.h"

// media files hosted on a local DLNA server, e.g.: 
#define MEDIA_FILE "http://192.168.1.11:9000/disk/DLNA-PNMP3-OP01-FLAGS01700000/O0$1$8I2534412.mp3"
//#define MEDIA_FILE "http://192.168.1.40:9790/*/H*c3*b6rb*c3*bccher/A01/A*20C*20M/1-01*20Titel*2001.mp3"

const char ssid[] = "MySSID";
const char pass[] = "MyPassword"; 

TLV320DAC3101 dac;
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
  Serial.begin(115200);
  Serial.println();
  Serial.println("running example \"Play-Local-Media\":");

  // connecting to local WiFi network
  Serial.printf("connecting to WiFi network \"%s\"\n", ssid);
  WiFi.begin(ssid, pass);
  while ( WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(2000);
  }
  Serial.printf("\nconnected successfully to \"%s\". IP address: %s\n", 
                   ssid, WiFi.localIP().toString().c_str());

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

  if (!dac.setPLLValues(1, 2, 32, 0)) {      // Configure PLL dividers P, R, J and D
    halt("Failed to configure PLL values!");
  }

  if (!dac.setNDAC(true, 8) ||               // Configure DAC dividers NDAC, MDAC and DOSR
      !dac.setMDAC(true, 2) ||
      !dac.setDOSR(128)) {
    Serial.println("Failed to configure DAC dividers!");
  }

  if (!dac.powerPLL(true)) { // Power up the PLL
    halt("Failed to power up PLL!");
  }

  // DAC Setup
  if (!dac.setDACDataPath(true, true,                      // Power up both DACs
                          TLV320_DAC_PATH_NORMAL,          // Normal left path
                          TLV320_DAC_PATH_NORMAL,          // Normal right path
                          TLV320_VOLUME_STEP_1SAMPLE)) {   // Step: 1 per sample
    halt("Failed to configure DAC data path!");
  }

  if (!dac.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER,   // Left DAC to mixer
                                 TLV320_DAC_ROUTE_MIXER,   // Right DAC to mixer
                                 false, false, false,      // No AIN routing
                                 false)) {                 // No HPL->HPR
    halt("Failed to configure DAC routing!");
  }

  // DAC Volume Control
  if (!dac.setDACVolumeControl(
        false, false, TLV320_VOL_INDEPENDENT) ||   // Unmute both channels
      !dac.setChannelVolume(false, 10) ||          // Left DAC +10dB
      !dac.setChannelVolume(true, 10)) {           // Right DAC +10dB
    halt("Failed to configure DAC volumes!");
  }

  // Headphone and Speaker Setup
  if (!dac.configureHeadphoneDriver(
        true, true,                           // Power up both drivers
        TLV320_HP_COMMON_1_65V,               // Default common mode
        false) ||                             // Don't power down on SCD
      !dac.configureHPL_PGA(0, true) ||       // Set HPL gain, unmute
      !dac.configureHPR_PGA(0, true) ||       // Set HPR gain, unmute
      !dac.setHPLVolume(true, 10) ||          // Enable and set HPL volume to -5dB
      !dac.setHPRVolume(true, 10)) {          // Enable and set HPR volume to -5dB
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
  audio.setVolume(10);                 // 0...21(max)
  audio.setConnectionTimeout(500,0);   // needed for some servers
  audio.connecttohost(MEDIA_FILE);
  delay(100);
}

void loop()
{
  audio.loop();              // play audio stream
  vTaskDelay(1);             // needed for ESP32-audioI2S lib!
}



