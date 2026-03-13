/*
    In this example, a playlist of all (audio) files with the specified file extensions is created from an SD card
    and their titles are played back via the ESP32-audioI2S library.

    Unlike most implementations, the SdFatFS library is used here instead of the SD or SD_MMC libs.
    
    The following Arduino Libraries are needed:
     - TLV320DAC3101 (>= V1.2.0.)
     - Adafruit TLV320 I2S
     - SdFat
     - ESP32-audioI2S  (https://github.com/schreibfaul1/ESP32-audioI2S.git)
     - SdFatFS (https://github.com/anp59/SdFatFS.git)
     
    The SdFatFS library implements the functions of the Arduino FS interface based on the SdFat solution from Greimann
    (https://github.com/greiman/SdFat) and can therefore be used as an alternative to the SD or SD_MMC libraries
    in order to use additional SdFat functionalities in projects.

    Using the SdFat library allows you to use only the so-called directory index instead of the file names
    of the files (short int for FAT32). This allows the time required to create of the playlist
    can be considerably shortened: Read 80 dirs with 1774 files in 617 ms (tested with 50 MHz SPI_CLK)
    The internal structure of the playlist also allows each directory path to be saved only once.

    The terminal can be used to navigate through the playlist and control the playback volume.
    The control commands via terminal are:
        Space bar -> next song
        Enter/Return key -> repeat current song
        Entering a decimal number -> set offset to next song (positive value: forwards - negative value: backwards)
        '-' Volume down
        '+' Volume up
        'l' / 'r' Balance
        's' sets speaker on/off

    Note: As when using the SD library, the SD card must be operated with SdFatFS via SPI.
    When using the YB-ESP32-S3-DAC board from yellobyte (https://github.com/yellobyte/YB-ESP32-S3-DAC)
    the solder bridge SD_CS must be closed [default].

    Last updated 2026-03-13, anp59
*/

#include <Arduino.h>
#include "SD_SDFAT.h"
#include "Audio.h"  // Audio.h should included after SD_SDFAT to avoid compiler warnings
#include "SdFatPlayList.h"
#include "TLV320DAC3101.h"

SPIClass SD_SPI(FSPI);  // GPIOs: MOSI=11, CLK=12, MISO=13, CS=10

TLV320DAC3101 dac;
tlv320_init_config_t cfg;

Audio audio;
SdFatPlayList plist;

void my_audio_info(Audio::msg_t m); // Callback function for displaying information from the audio lib during audio.loop()

bool playNextFile(int offset = 1);  // select next file from playlist (current file + offset)
void halt(const char *message);     // helper function to halt on critical errors
bool setBalance(int balance);       // implements balance function for TLV-DAC, maybe candidate for the TLV lib
bool setVolume(int vol, bool left_channel = true, bool right_channel = true);

// helper function for calculating values for TLV volume control
template <typename T>
T getValue(T min, T max, T cur, T delta);


const int volume_steps      = 32;   // volume 0...volume_steps (0..256, TLV uses max. 128 steps)
const int default_volume    = 20;
const int balance_steps     = 32;   // balance: -balance_steps...0...+balance_steps (TLV uses max. 32 steps)
const int default_balance   = 0;


const char *dir = "/";          // root dir for the playlist
const int subdirLevels = 10;    // subdirLevels > 0 : add files in dir files from all subdirs down to a depth of subdirLevels to the list, subdirLevels = 0 adds only files from dir to playlist.
bool f_eof = true;


/**************************************************/

void setup() {
    Serial.begin(115200);
    // SPI CLK 50 MHZ was successfully tested with the YB-ESP32-S3-DAC board. 
    // If problems occur, the CLK frequency should be reduced to 25 or 16 MHz.
    if ( !SDF.begin(SdSpiConfig(SS, DEDICATED_SPI, SD_SCK_MHZ(50), &SD_SPI)) ) {
        log_e("Card Mount failed!");
        return;
    }

    Serial.println(audio.getVersion());     // shows audio-lib version

    // HW reset makes sure DAC chip is reset properly
    pinMode(TLV_RESET, OUTPUT);
    digitalWrite(TLV_RESET, LOW);
    delay(10);
    digitalWrite(TLV_RESET, HIGH);

    // TLV320DAC3101 Audio DAC initialization
    cfg.sample_frequency = (float)audio.getSampleRate();    // Hz, must be defined
    cfg.dac_gain_left = -3.0;                               // dB, defaults to 0dB when not set,
    cfg.dac_gain_right = -3.0;                              // allowed range: -63.5...+24.0 dB

    if (!dac.initDAC(&cfg)) { halt(dac.getLastError().c_str()); }

    // activate headphone output and set headphone volume
    if(!dac.configHeadphoneOutput(true,   // enable headphone output
                                  false,  // HP(L/R) output driver acts as headphone driver
                                  100)) { // set volume (allowed range: 0(quiet)...127(loud))
        halt("Failed to configure headphone output!");
    }

    // activate speaker output and set speaker volume
    if (!dac.configSpeakerOutput(true,  // enable speaker output
                                 90)) { // set volume (allowed range: 0(quiet)...127(loud))
        halt("Failed to configure speaker output!");
    }

    // set default volume
    if (!setVolume(default_volume))
        halt("Failed to set speaker and / or headphone volume!");

    Serial.println("TLV320 DAC config done!");

    // audio-lib initialization
    audio.audio_info_callback = my_audio_info;
    audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
    audio.setVolume(17); // 0...21(max)
    audio.setTone(0, 0, 0);

    // create playlist
    plist.setFileFilter( {"mp3", "ogg", "wav", "aac", "flac", "m4a", "opus", "mp4"} );   // optional list consist of the  extensions of files to be considered for the playlist. Empty list = all file types
    uint32_t start = millis();
    uint32_t end = start;
    plist.createPlayList(dir, subdirLevels);
    end = millis()-start;

    if (plist.files.empty()) {
        log_e("No files in playlist!");
        f_eof = false;
        return;
    }

    // print info message for the example
    log_i(
"\n\n\
Playlist created: %d dirs with %d files in %lu ms.\n\
Playlist navigation:\n\
    Space -> next song\n\
    Enter -> repeat current song\n\
    Decimal number -> offset to next song (positive value: forwards - negative value: backwards)\n\
    '-' Volume down\n\
    '+' Volume up\n\
    'l' / 'r' Balance L/R \n\
    's' Speaker on/off\n", plist.dirs.size(), plist.files.size(), end );

    // play first file of the playlist (index 0)
    f_eof = !playNextFile(0);
}


/**************************************************/

void loop() {
    int offset = 1;
    char c = 0;
    static int cur_volume = default_volume;
    static int cur_balance = default_balance;
    static bool speakerEnabled = true;
    const char* cmds = "+-lrs";     // volume + -, baralance l r, speaker on/off s
    bool isCmd = false;

    audio.loop();
    // control next song
    if (Serial.available()) {
        c = Serial.read();
        isCmd = (strchr(cmds, c) != nullptr);
        if ( !isCmd ) {
            String s(c);
            s += Serial.readString();
                offset = s.toInt();
        }
        if ( !isCmd ) {
            audio.stopSong();
            f_eof = true;
        }
    }
    if (f_eof) {
        f_eof = !playNextFile(offset);
        vTaskDelay(150);
    }

    if  ( c == '+' || c == '-') {
        // volume control
        cur_volume = getValue(0, volume_steps-1, (int)cur_volume, (c == '+') ? 1 : -1);
        log_w("cur_volume = %d", cur_volume);
        setVolume(cur_volume);
    }
    if (c == 'l' || c == 'r') {
        // balance control
        cur_balance = getValue(-balance_steps, balance_steps, cur_balance, (c == 'r') ? 1 : -1);
        log_w("cur_balance = %d", cur_balance);
        setBalance((cur_balance));    // -32..0..+32
    }
    if ( c == 's' ) {
        dac.enableSpeaker(speakerEnabled = !speakerEnabled);
        log_w("speaker is %s!", !speakerEnabled ? "powered down" : "powered up");
    }

    vTaskDelay(1);
}

/****************************************************/

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
    if ( m.e == Audio::evt_eof ) f_eof = true;
}


bool playNextFile(int offset) {
    static int cur_pos = 0;
    const char *file_path;
    if ( plist.files.size() ) {
        cur_pos = modulo(cur_pos += offset, plist.files.size());
        if ( (file_path = plist.getFilePathAtIdx(cur_pos)) != nullptr ) {
            if ( audio.connecttoFS(SDF, file_path) ) {
                Serial.printf("\n**** now playing at [%d]: %s\n", cur_pos, file_path);
                return true;
            }
            else
                log_e("connectToSD failed: %s\n", file_path);
        }
    }
    return false;
}


void halt(const char *message) {
  Serial.println(message);
  while (true) yield();
}


template <typename T>
T getValue(T min, T max, T cur, T delta) {
    if ( min > max) return cur;
    if (delta >= 0) {
        return ( (cur + delta) <= max) ? cur + delta : max;
    } else {
        return ( (cur + delta) >= min) ? cur + delta : min;
    }
}


bool setBalance(int balance) {      // step 0.5 dB: -16 db ... 0 ... 16 db --> -32....0...+32
                                    // (balance>0: emphasize on the right → quieter on the left, balance<0, emphasize on the left → quieter on the right)
    float gain = min(max(balance, -32), 32) / 2.0;
    log_d("gain: L %1.2f <-> %1.2f R", gain, cfg.dac_gain_left - gain , cfg.dac_gain_right + gain);
    return (dac.setChannelVolume(false, cfg.dac_gain_left - gain) && // left channel
            dac.setChannelVolume(true, cfg.dac_gain_right + gain));
}


bool setVolume(int vol, bool left_channel, bool right_channel) {
    static uint16_t steps = 128;
    static float factor = 1.0;
    if(volume_steps != 0) {
        steps = (volume_steps > 256) ? 256 : volume_steps;
        factor = 128.0 / steps;
    }
    //uint8_t regVal = (vol == 0) ? 127 : uint8_t(128 - (vol * factor + factor));
    uint8_t regVal = uint8_t(vol * factor + factor);
    log_d("volume %d = regval 0x%02X, factor = %1.5f", vol, regVal, factor);
    return (dac.setHeadphoneVolume(regVal, left_channel, right_channel) &&
            dac.setSpeakerVolume(regVal, left_channel, right_channel));
}

