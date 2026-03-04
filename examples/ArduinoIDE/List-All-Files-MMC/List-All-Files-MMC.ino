/*
  List-All-Files-MMC

  This example prints all files on the microSD card. We use library "SD_MMC" which does not require the CS pin 
  routed to GPIO10. Therefore solder bridge SD_CS can be opened to free GPIO10 for other usage.
  
  More info about SD_MMC library is provided here: 
    "https://github.com/espressif/arduino-esp32/blob/master/libraries/SD_MMC"

  The microSD card slot on the YB-ESP32-S3-DAC DevBoard is connected to the fast SPI bus (FSPI) as follows:
     CS - GPIO10, MOSI - GPIO11, SCK - GPIO12, MISO - GPIO13

  Additional info:
  The ESP32-S3 includes four SPI controllers: SPI0, SPI1, SPI2(Fast SPI) and SPI3. SPI0/1 are reserved for
  Flash and PSRAM (if available) and should be left alone. The remaining two are available for the public. 
  In Arduino SPI2 & SPI3 are named FSPI & HSPI. FSPI by default is assigned to GPIOs 10-15 (IO MUX). However, 
  both can get pinned to any available GPIO pin if needed but will be slower if routed through GPIO Matrix.

  Last updated 2026-01-04, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD_MMC.h>
 
File root;

void printDirectory(File dir, int numTabs) {
	File entry;
  while(true) {
    entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print("   ");
    }

    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } 
    else {
      Serial.print("   ");
      // entry.size() returns file size in bytes and 0 for directories
      Serial.println(entry.size());
    }
    entry.close();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("running example \"List-All-Files-MMC\":");
  Serial.print("mounting SD card...");

  // The SD_MMC default definitions for MOSI(SD_MMC: CMD), SCK & MISO(SD_MMC: D0) are 11, 12 & 13 
  // (see pins_arduino.h for more info)
  SD_MMC.setPins(SCK, MOSI, MISO);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("error!");
    return;
  }
  Serial.println("successful");
	
  uint8_t cardType = SD_MMC.cardType();
  Serial.print("card type detected: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } 
  else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } 
  else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  }
   else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

  Serial.println();
  Serial.println("scanning files on SD card:");

  // print microSD content starting with root
  root = SD_MMC.open("/");
  printDirectory(root, 0);

  Serial.println();
  Serial.println("all done");
}

void loop()
{
  // 
}
