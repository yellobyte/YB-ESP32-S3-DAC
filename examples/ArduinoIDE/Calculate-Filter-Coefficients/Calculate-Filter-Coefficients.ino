/*
  The example demonstrates how to calculate TLV320DAC3101 IIR & BiQuad filter coefficients 
  for various sample frequencies, filter types and filter parameters.

  The following libraries are needed:
   - Adafruit_TLV320_I2S
   - Adafruit_BusIO
   - TLV320DAC3101

  Last updated 2026-02-20, ThJ <yellobyte@bluewin.ch>
*/

#include <Arduino.h>
#include "TLV320DAC3101.h"

TLV320DAC3101 dac;

void halt(const char *message) {
  Serial.println(message);
  while (true) yield();       // halt on critical errors
}

void setup() {
  Serial.begin(115200);

  Serial.println("running example \"Calculate TLV320DAC3101 Filter Coefficients\":");
  Serial.println();

  // IIR (1st order) low pass filter
  #define LPF1_FSAMPLE 44100
  tlv320_filter_param_t lpf1;

  lpf1.gain = -3.0;  // dB, filter gain
          
  Serial.println();
  Serial.printf("IIR Low Pass Filter Coefficients for fs=%.1fHz and gain=%.1fdB:\n",
                (float)LPF1_FSAMPLE, lpf1.gain);
  Serial.printf("fc       N0      N1      N2      D1      D2\n");
  for (float fc = 100.0; fc <= (float)LPF1_FSAMPLE / 2 - 100; fc += 100.0) {
    lpf1.fc = fc;
    if (!dac.calcDACFilterCoefficients((float)LPF1_FSAMPLE, TLV320_FILTER_TYPE_LOW_PASS,
                                       TLV320_FILTER_IIR, &lpf1)) {
      halt("Calculating filter coefficients failed! Please check your chosen filter parameters.");
    }
    Serial.printf("%5.0fHz  0x%02x%02x  0x%02x%02x  na      0x%02x%02x  na\n",
                  fc, lpf1.N0H, lpf1.N0L, lpf1.N1H, lpf1.N1L, lpf1.D1H, lpf1.D1L);
  }
  delay(200);

  // BiQuad (2nd order) low pass filter
  #define LPF2_FSAMPLE 44100

  tlv320_filter_param_t lpf2;
  
  Serial.println();
  Serial.printf("BiQuad Low Pass Filter Coefficients for fs=%.0fHz and gain=%.1fdB:\n",
                (float)LPF2_FSAMPLE, lpf2.gain);
  Serial.printf("fc       N0      N1      N2      D1      D2\n");
  for (float fc = 100.0; fc <= (float)LPF2_FSAMPLE / 2 - 100; fc += 100.0) {
    lpf2.fc = fc;
    if (!dac.calcDACFilterCoefficients((float)LPF2_FSAMPLE, TLV320_FILTER_TYPE_LOW_PASS,
                                       TLV320_FILTER_BIQUAD, &lpf2)) {
      halt("Calculating filter coefficients failed! Please check your chosen filter parameters.");
    }
    Serial.printf("%5.0fHz  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x\n",
                  fc, lpf2.N0H, lpf2.N0L, lpf2.N1H, lpf2.N1L, lpf2.N2H, lpf2.N2L,
                  lpf2.D1H, lpf2.D1L, lpf2.D2H, lpf2.D2L);
  }
  delay(200);

  // BiQuad (2nd order) high pass filter
  #define HPF_FSAMPLE 48000

  tlv320_filter_param_t hpf;

  hpf.gain = 1.0;  // dB, filter gain
  
  Serial.println();
  Serial.printf("BiQuad High Pass Filter Coefficients for fs=%.0fHz and gain=%.1fdB:\n",
                (float)HPF_FSAMPLE, hpf.gain);
  Serial.printf("fc       N0      N1      N2      D1      D2\n");
  for (float fc = 100.0; fc <= (float)HPF_FSAMPLE / 2 - 100; fc += 100.0) {
    hpf.fc = fc;
    if (!dac.calcDACFilterCoefficients((float)HPF_FSAMPLE, TLV320_FILTER_TYPE_HIGH_PASS,
                                       TLV320_FILTER_BIQUAD, &hpf)) {
      halt("Calculating filter coefficients failed! Please check your chosen filter parameters.");
    }
    Serial.printf("%5.0fHz  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x\n",
                  fc, hpf.N0H, hpf.N0L, hpf.N1H, hpf.N1L, hpf.N2H, hpf.N2L,
                  hpf.D1H, hpf.D1L, hpf.D2H, hpf.D2L);
  }
  delay(200);

  // BiQuad (2nd order) notch filter
  #define NOTCH_FSAMPLE 32000
  tlv320_filter_param_t notch;

  notch.bw = (float)200;  // Hz, -3dB bandwidth
  notch.gain = 2.0;       // dB, filter gain
  
  Serial.println();
  Serial.printf("BiQuad Notch Filter Coefficients for fs=%.0fHz, bandwidth=%.0fHz and gain=%.1fdB:\n",
                (float)NOTCH_FSAMPLE, notch.bw, notch.gain);
  Serial.printf("fc       N0      N1      N2      D1      D2\n");
  for (float fc = 100.0; fc <= (float)NOTCH_FSAMPLE / 2 - 100; fc += 100.0) {
    notch.fc = fc;
    if (!dac.calcDACFilterCoefficients((float)NOTCH_FSAMPLE, TLV320_FILTER_TYPE_NOTCH,
                                       TLV320_FILTER_BIQUAD, &notch)) {
      halt("Calculating filter coefficients failed! Please check your chosen filter parameters.");
    }
    Serial.printf("%5.0fHz  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x\n",
                  fc, notch.N0H, notch.N0L, notch.N1H, notch.N1L, notch.N2H, notch.N2L,
                  notch.D1H, notch.D1L, notch.D2H, notch.D2L);
  }
  delay(200);

  // BiQuad (2nd order) peakingEQ filter
  #define EQ_FSAMPLE 44100
  tlv320_filter_param_t eq;
  eq.bw = (float)600;  // Hz, bandwidth
  eq.gain = 10.0;      // dB, gain
  
  Serial.println();
  Serial.printf("BiQuad EQ Filter Coefficients for fs=%.0fHz, bandwidth=%.0fHz and gain=%.1fdB:\n",
                (float)EQ_FSAMPLE, eq.bw, eq.gain);
  Serial.printf("fc       N0      N1      N2      D1      D2\n");
  for (float fc = 100.0; fc <= (float)EQ_FSAMPLE / 2 - 100; fc += 100.0) {
    eq.fc = fc;
    if (!dac.calcDACFilterCoefficients((float)EQ_FSAMPLE, TLV320_FILTER_TYPE_EQ,
                                       TLV320_FILTER_BIQUAD, &eq)) {
      halt("Calculating filter coefficients failed! Please check your chosen filter parameters.");
    }
    Serial.printf("%5.0fHz  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x\n",
                  fc, eq.N0H, eq.N0L, eq.N1H, eq.N1L, eq.N2H, eq.N2L,
                  eq.D1H, eq.D1L, eq.D2H, eq.D2L);
  }  
  delay(200);

  // BiQuad (2nd order) treble shelf filter
  #define SHELF_FSAMPLE 48000
  tlv320_filter_param_t shelf;
  shelf.gain = 10.0;  // dB, gain
  
  Serial.println();
  Serial.printf("Treble Shelf Filter Coefficients for fs=%.0fHz and gain=%.1fdB:\n",
                (float)SHELF_FSAMPLE, shelf.gain);
  Serial.printf("fc       N0      N1      N2      D1      D2\n");
  for (float fc = 100.0; fc <= (float)SHELF_FSAMPLE / 2 - 100; fc += 100.0) {
    shelf.fc = fc;
    if (!dac.calcDACFilterCoefficients((float)SHELF_FSAMPLE, TLV320_FILTER_TYPE_TREBLE_SHELF,
                                       TLV320_FILTER_BIQUAD, &shelf)) {
      halt("Calculating filter coefficients failed! Please check your chosen filter parameters.");
    }
    Serial.printf("%5.0fHz  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x  0x%02x%02x\n",
                  fc, shelf.N0H, shelf.N0L, shelf.N1H, shelf.N1L, shelf.N2H, shelf.N2L,
                  shelf.D1H, shelf.D1L, shelf.D2H, shelf.D2L);
  }

  Serial.println("Finished.");
}

void loop() {
  //
}

 

 
