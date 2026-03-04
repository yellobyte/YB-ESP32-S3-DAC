## YB-ESP32-S3-DAC Development Board Overview:
The **YB-ESP32-S3-DAC** is an audio development board based on Espressif's ESP32-S3 MCU with **Stereo Headphone** and **Stereo Speaker** output. It features an **ESP32-S3-WROOM-1-N8R2** module (8MB Flash, 2MB PSRAM, WiFi PCB antenna) and is available on sales platforms [eBay](https://www.ebay.de/sch/i.html?_nkw=yb-esp32-s3) and [Ricardo.ch](https://www.ricardo.ch/en/s/YB-ESP32-S3).

The board is packed with features. It provides a **Texas Instruments TLV320 Stereo Audio DAC** with audio processing capability and integrated **Stereo Class-D Speaker Amplifier**, **3.5mm Stereo Headset Socket**, **microSD** card slot, **CH334 USB-Hub** chip, **CH343 USB-UART bridge** chip, **USB-C connector** for software upload, serial output and/or feeding power to the board, **two status LEDs** and lots of **GPIO pins** for free use.  

<p align="center"><img src="https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc/YB-ESP32-S3-DAC_top.jpg" height="160"/></p>  

For a quick start **connect a 8Ω loudspeaker** or a **headphone** to the board, apply power, build and upload an example and off you go. You quickly can listen to **internet radio stations**, **play audio files from microSD card** and much more.  

You can connect additional hardware to the board, e.g. TFT displays, IR receivers or any other module that communicates via I2C/SPI, etc. Please have a look at the provided [examples](https://github.com/yellobyte/YB-ESP32-S3-DAC/tree/main/examples) for PlatformIO resp. ArduinoIDE. 

The densly populated YB-ESP32-S3-DAC board provides multiple GPIO pins and is still highly [**breadboard compatible**](https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc/YB-ESP32-S3-DAC_on_breadboard.jpg) for it leaves one row of accessible breadboard contacts on either side of the board. All I/O ports (GPIOx) are clearly labeled on both sides of the board. 

## YB-ESP32-S3-DAC board features in detail:
- **ESP32-S3-WROOM-1-N8R2** module with 8MB Flash, 2MB PSRAM, WiFi PCB antenna
- **TLV320DAC3101 Stereo Audio DAC** with audio processing capability (audio filters of 1st and 2nd order, dynamic range compression DRC), Stereo Class-D Speaker Amplifier (2 x 1.3W@8Ω) and Stereo Headphone output (2 x 65mW@16Ω impedance min.). The DAC is connected to the ESP32-S3 as follows:
  - Via I2C for DAC configuration & programming (I2C address 0x18):
    - *GPIO8 - SDA* (data line)
    - *GPIO9 - SCL* (clock line)
  - Via I2S for transferring audio data from ESP32-S3 to DAC:
    - *GPIO4 - MCLK* (master clock, optional when solder bridge is closed)
    - *GPIO5 - BCLK* (bit clock)
    - *GPIO6 - LRCLK/WCLK* (frame/word clock)
    - *GPIO7 - DIN* (digital audio signal)  

  Note: GPIOs 5/6/7 are not wired to a board pin, however they are available via labeled solder pins on the bottom of the board.
 - **JST PH2.0 connectors** for easy connecting 2 loudspeakers (8Ω impedance, left & right audio channels)
 - **microSD** card slot connected to the ESP32-S3 via fast SPI bus *FSPI*:
   - *GPIO10 - SCS* (SPI bus control, chip select, this control line is not needed for SD_MMC-lib and available for other usage when solder bridge *SD_CS* is open [default closed])
   - *GPIO11 - MOSI* (SPI bus data communication, SD_MMC calls it *CMD*)
   - *GPIO12 - SCK* (SPI bus clock signal)
   - *GPIO13 - MISO* (SPI bus data communication, SD_MMC calls it *D0*)  

   Note: GPIOs 11/12/13 are not wired to any board pin, they are exclusively used for the microSD slot.
 - **control LEDs**. One LED labeled 'P' is connected to the 3.3V rail to indicate board power and the other LED labeled 'IO47' is connected to GPIO47 which can be used as status LED.  
 - **USB-C** port connected to the onboard CH334 USB-Hub chip. This allows for serial output and simultaneous JTAG debugging as well as software upload (e.g. via ArduinoIDE, VSCode/PlatformIO etc). The ESP32-S3 contains an inbuild JTAG adapter hence [**debugging**](https://github.com/yellobyte/ESP32-DevBoards-Getting-Started/tree/main/debugging) becomes fairly easy.
 - **hardware logic** for *automatic* software upload (supported by most Development IDEs) via USB-C port using the onboard CH343 USB-UART bridge chip. How this works is explained [here](https://github.com/yellobyte/ESP32-DevBoards-Getting-Started/tree/main/reset_and_software_upload).  
 - **pushbuttons**. One is labeled 'R' and resets the ESP32-S3 (shorts EN pin to ground) and the other one is labeled 'B' and shorts GPIO0 to ground when pressed. The combination is used to force the board into boot mode.
 - **lots of available GPIOs** for connecting LEDs, buttons, additional modules via second SPI bus or I2C, e.g displays, rotary encoders, bluetooth, etc.

## Board Pin Layout:
 ![](https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc/YB-ESP32-S3-DAC_pinlayout.jpg)

The boards **outline** and **schematic** files are all located in folder [doc](https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc), together with data sheets for Espressif's MCU ESP32-S3 and the ESP32-S3-WROOM-1 module family.

## Powering the board:
There are two ways to provide power to the board:
  - through the USB-C port or
  - ~5VDC applied to the 5V pin  

The board uses LDOs to drop the external supply voltage down to 3.3V/1.8V needed by the board circuitry. The TLV320 speaker drivers get their voltage directly from pin '5V'. They can operate up to 5.5V and their absolute maximum rating is 6V. Therefore **never supply more than ~5.5VDC to the '5V' power input pins** !  

Normal operating current of the idle board (all GPIOs unconnected, no audio output, WiFi disabled) is about 45mA. With WiFi active the board draws about 100mA (mainly depending on WiFi link). With WiFi active, a microSD in the slot and lower audio output power on both amp channels the current rises to ~160mA.

### Recommended wiring for max. audio output power:

Each speaker channel produces max. 1.3W output power at 8Ω. If your project requires more then just a few ~100mW of audio output power it is recommended to use a capable power supply (5VDC/1A). An additional capacitor of ~100uF or greater value across the 5V/GND pins of the board wouldn't hurt either.

## Application hints: 
The board uses a WCH chip CH343P (USB-UART bridge). If you haven't done yet then you need to install the CH343 Driver on your Laptop/PC. For Windows go [here](https://www.wch-ic.com/search?t=all&q=ch343) and download and install the newest version of the driver. Linux provides CH34x drivers by default. 

### Arduino IDE:
As of Arduino ESP32 Core V3.3.6 you open the board list, enter "yb" and then select "**Yellobyte YB-ESP32-S3-DAC**". Now choose the proper settings for COM port, debug level, etc. as shown below. Be aware, since the ESP32-S3 MCU is very versatile there are a lot of build options to play with. Espressif's homepage offers some help.

Correct **ArduinoIDE** settings for the **YB-ESP32-S3-DAC** board:  
- Board: *YB-ESP32-S3-DAC*
- USB CDC On Boot: _Disabled_
- Flash Size: *8MB (64Mb)*
- Partition Scheme: *8MB with spiffs (...)*
- PSRAM: *QSPI PSRAM*  

 ![](https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc/YB-ESP32-S3-DAC_ArduinoIDE-Settings.jpg) 

  
### PlatformIO:
Building with **PlatformIO** is easy as well. Starting with Arduino ESP32 Core v3.3.6 the VSCode/PlatformIO IDE holds the necessary *.json board file which provides the correct board definitions & settings.  

Just create a new project and give it a name, then go to board selection, enter "yb-" and choose your YB-ESP32-S3-*** board from the list that is popping up.

 ![](https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc/YB-ESP32-S3-DAC_PlatformIO_board_selection.jpg)

Examples that need to be build with an older framework still come with a folder "boards" which keeps the necessary *.json board definition files. 

### Software Upload to the board:

Uploading new software to boards with your IDE is a breeze. Select the correct COM port and upload the program. The integrated hardware logic will put the board into upload mode automatically.

However, at any time and if needed you can force the ESP32-S3 into upload mode *manually*. Keep the **'B'** button pressed, then press/release the **'R'** button and finally release the **'B'** button. The serial monitor output will subsequently confirm the boards readiness for getting flashed.  

```
....
23:19:07.453 > ESP-ROM:esp32s3-20210327
23:19:07.453 > Build:Mar 27 2021
23:19:07.453 > rst:0x1 (POWERON),boot:0x0 (DOWNLOAD(USB/UART0))
23:19:07.459 > waiting for download
```

### Flash/RAM usage:

The -N8R2 module on the board should provide enough memory even for demanding projects. Building e.g. software example [Play-All-MP3-Files-on-SD](https://github.com/yellobyte/YB-ESP32-S3-DAC/tree/main/examples/ArduinoIDE/Play-All-MP3-Files-on-SD) shows the following Flash/RAM usage:  
```
Executing task: C:\Users\tj\.platformio\penv\Scripts\platformio.exe run 

Processing v2 (board: yb-esp32-s3-amp-v2; platform: espressif32; framework: arduino)
------------------------------------------------------------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/yb-esp32-s3-amp-v2.html
PLATFORM: Espressif 32 (6.9.0) > YB-ESP32-S3-AMP (8 MB QD FLASH, 2 MB PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 8MB Flash
DEBUG: Current (esp-builtin) On-board (esp-builtin) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 3.20017.0 (2.0.17) 
 - tool-esptoolpy @ 1.40501.0 (4.5.1) 
 - toolchain-riscv32-esp @ 8.4.0+2021r2-patch5 
 - toolchain-xtensa-esp32s3 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 62 compatible libraries
Scanning dependencies...
Dependency Graph
|-- SD @ 2.0.0
|-- ESP32-audioI2S @ 2.0.0
|-- FS @ 2.0.0
|-- SPI @ 2.0.0
Building in release mode
Retrieving maximum program size .pio\build\v2\firmware.elf
Checking size .pio\build\v2\firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [=         ]   8.4% (used 27608 bytes from 327680 bytes)
Flash: [===       ]  28.5% (used 950957 bytes from 3342336 bytes)
```
### General notes:

1) In case you get left channel output on the right speaker and vice-versa (L + R channels are swapped) you need to build with a [newer ESP32-AudioI2S library version](https://github.com/schreibfaul1/ESP32-audioI2S/releases) with this error fixed.

2) The ESP32-S3 includes four SPI controllers: SPI0, SPI1, SPI2(Fast SPI) and SPI3. SPI0/1 are reserved for Flash and PSRAM (if available) and should be left alone!  
The remaining two are available for the public. In the Arduino universe SPI2 & SPI3 are named FSPI & HSPI. FSPI by default is assigned to GPIOs 10-15 and routed via IO MUX. However, both can get pinned to any available GPIO pin if needed but FSPI will be slower if routed through GPIO Matrix

3) The ESP32-S3-WROOM-1 module family comprises several [**versions**](https://github.com/yellobyte/ESP32-DevBoards-Getting-Started/raw/main/ESP32_specs_and_manuals/ESP32-S3-WROOM-1(U)_Variants.jpg). The **-1** versions come with embedded PCB antenna, the **-1U** versions with IPEX antenna socket instead. The extension -Nx(Ry) defines the amount of integrated FLASH/PSRAM, e.g. -N4 (4MB Flash, no PSRAM), -N4R2 (4 MB Flash, 2MB PSRAM), -N8R2 (8 MB Flash, 2MB PSRAM) etc.  

5) The board supports USB Serial JTAG debugging. An example is provided [here](https://github.com/yellobyte/YB-ESP32-S3-DAC/tree/main/examples/PlatformIO/Debug-via-builtin-JTAG). 

### Integrating this board into your own PCB design projects:

Its easy. Folder [doc](https://github.com/yellobyte/YB-ESP32-S3-DAC/tree/main/doc) provides the Eagle library file **_yb-esp32-S3-DAC.lbr_** containing the board. Most other PCB design software (e.g. KiCad) are able to import and use Eagle lib files. 

<p align="center"><img src="https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc/Eagle_project_with_yb-esp32-S3-DAC.jpg" height="250"/>&nbsp;<img src="https://github.com/yellobyte/YB-ESP32-S3-DAC/raw/main/doc/Eagle_project_with_yb-esp32-S3-DAC2.jpg" height="250"/></p> 

## Final Remark for first usage:  
**>>> All YB-ESP32-S3-DAC boards delivered have already been flashed with software example 'Play-All-MP3-Files-on-SD'. <<<**  

For a quick board test just insert a microSD card (FAT32) into the socket holding some *.mp3 files, connect one (or two) speaker(s) or a headphone to the board and power it up (via USB or 5V pins).  

The status LED will light up when the microSD card has been detected successfully and the board will start playing all *.mp3 files found.
