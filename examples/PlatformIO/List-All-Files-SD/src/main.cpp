/*
  List-All-Files-SD

  This example prints the files on the microSD card located in the onboard
  microSD card slot. We use the "SD library" which requires GPIO10 (the
  SD-Card's CS signal) and therefore solder bridge SD_CS closed [default].

  More info about the SD library is provided here:
  "https://github.com/espressif/arduino-esp32/tree/master/libraries/SD"

  The microSD card slot on the YB-ESP32-S3-DAC DevBoard is connected to the fast
  SPI bus (FSPI) as follows: 
     CS   - GPIO10, MOSI - GPIO11, SCK  - GPIO12, MISO - GPIO13

  Additional info:
  The ESP32-S3 includes four SPI controllers: SPI0, SPI1, SPI2(Fast SPI) and
  SPI3. SPI0/1 are reserved for Flash and PSRAM (if available) and should be
  left alone. The remaining two are available for the public. In Arduino SPI2 &
  SPI3 are named FSPI & HSPI. FSPI by default is assigned to GPIOs 10-15 (IO
  MUX). However, both can get pinned to any available GPIO pin if needed but
  will be slower if routed through GPIO Matrix

  Last updated 2026-03-08, ThJ <yellobyte@bluewin.ch>
*/

#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Arduino.h>

SPIClass *spi_onboardSD = new SPIClass(FSPI);

File root;

void printDirectory(File dir, int numTabs) {
  File entry;
  while (true) {
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
    } else {
      Serial.print("   ");
      // entry.size() returns file size in bytes and 0 for directories
      Serial.println(entry.size());
    }
    entry.close();
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(115200);
  Serial.println();
  Serial.println("Running example \"List-All-Files-SD\":");

  spi_onboardSD->begin(SCK, MISO, MOSI, SS);

  Serial.print("trying to mount SD in onboard microSD card slot...");
  if (!SD.begin(SS, *spi_onboardSD)) {
    Serial.println("error!");
    return;
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("successful");

  /*   Serial.print("card type detected: ");
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
    } */

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("microSD Card Size: %lluMB\n", cardSize);

  Serial.println();
  Serial.println("scanning files on SD card:");

  // print microSD content starting with root
  root = SD.open("/");
  printDirectory(root, 0);

  Serial.println();
  Serial.println("all done");
}

void loop() {
  //
}
